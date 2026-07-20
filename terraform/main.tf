terraform {
    required_providers {
      aws = {
        source  = "hashicorp/aws"
        version = "~> 5.0"
      }

      local = {
        source  = "hashicorp/local"
        version = "~> 2.0"
      }
    }
    required_version = ">= 1.0"
}

provider "aws" {
    region      = var.aws_region
    access_key  = var.aws_access_key
    secret_key  = var.aws_secret_key
}

data "aws_iot_endpoint" "current" {
    endpoint_type = "iot:Data-ATS"
}

resource "aws_iot_thing" "dual_zone_climate_control" {
    name        = "dual-zone-climate-control"
}

resource "aws_iot_policy" "dual_zone_climate_policy" {
    name        = "dual-zone-climate-policy"

    policy = jsonencode({
        Version = "2012-10-17"
        Statement = [
            {
                Effect   = "Allow"
                Action   = "iot:Connect"
                Resource = "arn:aws:iot:${var.aws_region}:${var.aws_account_id}:client/dual-zone-climate-control"
            },
            {
                Effect   = "Allow"
                Action   = "iot:Publish"
                Resource = "arn:aws:iot:${var.aws_region}:${var.aws_account_id}:topic/home/dual-zone-climate/*"
            },
            {
                Effect   = "Allow"
                Action   = "iot:Subscribe"
                Resource = "arn:aws:iot:${var.aws_region}:${var.aws_account_id}:topicfilter/home/dual-zone-climate/*"
            },
            {
                Effect   = "Allow"
                Action   = "iot:Receive"
                Resource = "arn:aws:iot:${var.aws_region}:${var.aws_account_id}:topic/home/dual-zone-climate/*"
            },
            {
                Effect   = "Allow"
                Action   = "iot:Publish"
                Resource = "arn:aws:iot:${var.aws_region}:${var.aws_account_id}:topic/$aws/things/dual-zone-climate-control/shadow/*"
            },
            {
                Effect   = "Allow"
                Action   = "iot:Subscribe"
                Resource = "arn:aws:iot:${var.aws_region}:${var.aws_account_id}:topicfilter/$aws/things/dual-zone-climate-control/shadow/*"
            },
            {
                Effect   = "Allow"
                Action   = "iot:Receive"
                Resource = "arn:aws:iot:${var.aws_region}:${var.aws_account_id}:topic/$aws/things/dual-zone-climate-control/shadow/*"
            }
        ]
    })
}

resource "aws_iot_certificate" "dual_zone_climate_control_certificate" {
    active      = true
}

resource "local_file" "cert" {
    filename    = "${path.module}/../certs/certificate.pem.crt"
    content     = aws_iot_certificate.dual_zone_climate_control_certificate.certificate_pem
}

resource "local_file" "private_key" {
    filename    = "${path.module}/../certs/private.pem.key"
    content     = aws_iot_certificate.dual_zone_climate_control_certificate.private_key
}

resource "aws_iot_policy_attachment" "dual_zone_climate_policy_attachment" {
    policy      = aws_iot_policy.dual_zone_climate_policy.name
    target      = aws_iot_certificate.dual_zone_climate_control_certificate.arn
}

resource "aws_iot_thing_principal_attachment" "dual_zone_climate_thing_principal_attachment" {
    thing       = aws_iot_thing.dual_zone_climate_control.name
    principal   = aws_iot_certificate.dual_zone_climate_control_certificate.arn
}

resource "aws_iot_topic_rule" "dual_zone_climate_rule" {
    name        = "dual_zone_climate_rule"
    enabled     = true
    sql         = "SELECT * FROM 'home/dual-zone-climate/monitoring-device'"
    sql_version = "2016-03-23"

    lambda {
        function_arn = aws_lambda_function.dual_zone_climate_readings.arn
    }
}

resource "aws_lambda_permission" "iot_invoke_lambda" {
    statement_id    = "allow_iot_invoke_dual_zone_climate_readings"
    action          = "lambda:InvokeFunction"
    function_name   = aws_lambda_function.dual_zone_climate_readings.function_name
    principal       = "iot.amazonaws.com"
    source_arn      = aws_iot_topic_rule.dual_zone_climate_rule.arn
}

resource "aws_dynamodb_table" "dual_zone_climate_readings" {
    name         = "DualZoneClimateReadings"
    billing_mode = "PAY_PER_REQUEST"
    hash_key     = "device_id"
    range_key    = "timestamp"

    attribute {
        name = "device_id"
        type = "S"
    }

    attribute {
        name = "timestamp"
        type = "S"
    }

    ttl {
        attribute_name = "expiry_time"
        enabled        = true
    }
}

resource "aws_iam_role" "lambda_role" {
    name = "dual-zone-climate-control-lambda-role"

    assume_role_policy = jsonencode({
        Version = "2012-10-17"
        Statement = [
            {
                Action    = "sts:AssumeRole"
                Effect    = "Allow"
                Principal = {
                    Service = "lambda.amazonaws.com"
                }
            }
        ]
    })
}

resource "aws_iam_role_policy" "lambda_dynamodb" {
    role = aws_iam_role.lambda_role.name
    policy = jsonencode({
        Version = "2012-10-17"
        Statement = [
            {
                Effect   = "Allow"
                Action   = [
                    "dynamodb:PutItem",
                    "dynamodb:UpdateItem",
                    "dynamodb:GetItem",
                    "dynamodb:Query",
                    "dynamodb:Scan"
                ]
                Resource = aws_dynamodb_table.dual_zone_climate_readings.arn
            }
        ]
    })
}

resource "aws_iam_role_policy" "lambda_cloudwatch" {
    role = aws_iam_role.lambda_role.name
    policy = jsonencode({
        Version = "2012-10-17"
        Statement = [
            {
                Effect   = "Allow"
                Action   = [
                    "logs:CreateLogGroup",
                    "logs:CreateLogStream",
                    "logs:PutLogEvents",
                    "cloudwatch:PutMetricData"
                ]
                Resource = "*"
            }
        ]

    })
}

resource "aws_iam_role_policy_attachment" "lambda_basic" {
    role       = aws_iam_role.lambda_role.name
    policy_arn = "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"
}

resource "aws_lambda_function" "dual_zone_climate_readings" {
    filename         = "lambda_function.zip"
    function_name    = "dual-zone-climate-readings"
    role             = aws_iam_role.lambda_role.arn
    handler          = "lambda_function.lambda_handler"
    runtime          = "python3.12"
    source_code_hash = filebase64sha256("lambda_function.zip")
    timeout          = 10
    memory_size      = 128

    depends_on       = [
        aws_iam_role_policy.lambda_dynamodb,
        aws_iam_role_policy.lambda_cloudwatch,
        aws_iam_role_policy_attachment.lambda_basic
    ]
}
