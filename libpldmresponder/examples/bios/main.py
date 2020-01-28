import argparse
import sys
import jsonschema
import json
def handle_validation_error():
    sys.exit("validation failed");
def validate_schema(data,schema):
    with open(data) as data_handle:
        data_json=json.load(data_handle)
        with open(schema) as schema_handle:
            schema_json=json.load(schema_handle)
            try:
               jsonschema.validate(data_json, schema_json)
            except jsonschema.ValidationError as e:
                print(e)
                handle_validation_error()
    return data_json
if __name__ == '__main__':
    parser=argparse.ArgumentParser(description='BIOS related JSON config files validator')
    parser.add_argument('-s','--schema_file',dest='schema_file',help='JSON schema file')
    parser.add_argument('-c','--configuration_file',dest='configuration_file',help='JSON configuration file')
    args=parser.parse_args()
    if not args.schema_file:
        parser.print_help()
        sys.exit("Error: Schema file is required.")
    if not args.configuration_file:
        parser.print_help()
        sys.exit("Error: Configuration file is required.")
    data_json = validate_schema(args.configuration_file, args.schema_file)
