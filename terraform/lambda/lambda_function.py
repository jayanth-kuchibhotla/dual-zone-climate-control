import json
import boto3
from datetime import datetime, timezone

dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('DualZoneClimateReadings')
cloudwatch = boto3.client('cloudwatch')

def lambda_handler(event, context):
    try:
        device_id = event.get('device', 'unknown')
        timestamp = datetime.now(timezone.utc).isoformat()
        
        reported = event.get('state', {}).get('reported', {})
        zone_a = reported.get('zoneA', {})
        zone_b = reported.get('zoneB', {})

        # Write to DynamoDB
        table.put_item(Item={
            'device_id': device_id,
            'timestamp': timestamp,
            'zoneA_temp': str(zone_a.get('temperature',0)),
            'zoneA_humidity': str(zone_a.get('humidity',0)),
            'zoneA_relay1': zone_a.get('relay1', "OFF"),
            'zoneB_temp': str(zone_b.get('temperature',0)),
            'zoneB_humidity': str(zone_b.get('humidity',0)),
            'zoneB_relay2': zone_b.get('relay2',"OFF")
        })

        # Push custom metric to CloudWatch for alarm + Grafana
        cloudwatch.put_metric_data(
            Namespace='DualZoneClimateMonitor',
            MetricData=[
                {'MetricName': 'ZoneATemp', 'Value': float(zone_a.get('temperature', 0)), 'Unit': 'None'},
                {'MetricName': 'ZoneAHumidity', 'Value': float(zone_a.get('humidity', 0)), 'Unit': 'Percent'},
                {'MetricName': 'ZoneBTemp', 'Value': float(zone_b.get('temperature', 0)), 'Unit': 'None'},
                {'MetricName': 'ZoneBHumidity', 'Value': float(zone_b.get('humidity', 0)), 'Unit': 'Percent'}
            ]
        )

        return {
            'statusCode': 200,
            'body': json.dumps('Reading stored successfully')
        }

    except Exception as e:
        print(f"Error processing reading: {str(e)}")
        return {
            'statusCode': 500,
            'body': json.dumps(f'Error: {str(e)}')
        }