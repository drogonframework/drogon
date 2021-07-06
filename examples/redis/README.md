# Redis example

A simple redis example

## Usage

First of all you need redis running on the port 6379

### Post

```
curl --location --request POST 'localhost:8080/client/foo' \
--header 'Content-Type: application/json' \
--data-raw '{
  "value": "bar"
}'
```

### Get

```
curl --location --request GET 'localhost:8080/client/foo'
```
