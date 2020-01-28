import jsonschema
import json

with open('schema.json','r') as f:
    schema=json.loads(f.read()) 
		

input_file=input('Enter the data_file:')
with open(input_file,'r') as file:
    json_data=json.loads(file.read())
 

try:
    jsonschema.validate(json_data,schema)
except jsonschema.ValidationError as e:
    print(e.message)
except jsonschema.SchemaError as e:
    print (e)


