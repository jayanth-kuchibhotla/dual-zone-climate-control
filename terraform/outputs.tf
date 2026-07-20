output "dynamodb_table_name" {
    description = "DynamoDB table name"
    value       = aws_dynamodb_table.dual_zone_climate_readings.name
}

output "lambda_function_name" {
    description = "Lambda function name"
    value       = aws_lambda_function.dual_zone_climate_readings.function_name
}

output "iot_core_endpoint" {
    description = "AWS IoT Core device data endpoint for ESP32 firmware"
    value       = data.aws_iot_endpoint.current.endpoint_address
}
