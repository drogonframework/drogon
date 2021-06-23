This folder contains a [jsonstore](https://github.com/bluzi/jsonstore)-like storage service. But is multi-threaded and stores everything in memory. Serving as a showcase on how to build a minimally useful RESTful APIs in Drogon.

## API

#### /get-token
Generate a token that the user can use to create, read, modify and delete records.

* **method**: GET
* **URL params**: None
* **Body**: None
* **Success response**
  * **Code**: 200
  * **Content**: `{"token":"3a322920d42ef0763152a6efff2ed51985530aedd45370f92fd0f0b8dcc30220"}`
* **Sample call**

```bash
❯ curl -XGET http://localhost:8848/get-token
{"token":"3a322920d42ef0763152a6efff2ed51985530aedd45370f92fd0f0b8dcc30220"}
```

#### /{token}

Create a new JSON object associated with the token

* **method**: POST
* **URL params**: None
* **Body**: The inital JSON object to store
* **Success response**
  * **Code**: 200
  * **Content**: `{"ok":true}`
* **Failed response**:
  * **Code**: 500
  * **Why**: Either the token already is associated with the data or request body/content-type is not JSON.
  * **Content**: `{"ok":false}`
* **Sample call**

```bash
❯ curl -XPOST http://localhost:8848/3a322920d42ef0763152a6efff2ed51985530aedd45370f92fd0f0b8dcc30220 \
        -H 'content-type: application/json' -d '{"foo":{"bar":42}}'
{"ok":true}
```

Delete the JSON object associated with the token

* **method**: DELETE
* **URL params**: None
* **Body**: None
* **Success response**
  * **Code**: 200
  * **Content**: `{"ok":true}`
* **Sample call**

```bash
❯ curl -XDELETE http://localhost:8848/3a322920d42ef0763152a6efff2ed51985530aedd45370f92fd0f0b8dcc30220
{"ok":true}
```

#### /{token}/{some/path/to/data}

Retrieve data at and below the specified path
* **method**: GET
* **URL params**: None
* **Body**: None
* **Success response**
  * **Code**: 200
  * **Content**: `{"foo":{"bar":42}}`
* **Failed response**:
  * **Code**: 500
  * **Why**: No data associated with the provided token, or it does not exist in the JSON object.
  * **Content**: `{"ok":false}`
* **Sample call**

```bash
❯ curl -XGET http://localhost:8848/3a322920d42ef0763152a6efff2ed51985530aedd45370f92fd0f0b8dcc30220/
{"foo":{"bar":42}}
```

Update data at the specified path
* **method**: PUT
* **URL params**: None
* **Body**: The JSON object you wish to replace to
* **Success response**
  * **Code**: 200
  * **Content**: `{"ok":true}`
* **Failed response**:
  * **Code**: 500
  * **Why**: No data associated with the provided token or it does not exist in the JSON object.
  * **Content**: `{"ok":false}`
* **Sample call**

```bash
❯ curl -XPUT http://localhost:8848/3a322920d42ef0763152a6efff2ed51985530aedd45370f92fd0f0b8dcc30220/foo \
        -H 'content-type: application/json' -d '{"fruit":"apple"}'
{"ok":true}
```

## Example use

```bash
export URL="http://localhost:8848"
export TOKEN=`curl $URL/get-token -s | sed 's/.*"\([0-9a-f]*\)".*/\1/'`
printf "Token is: $TOKEN\n" 

printf 'Creating new data \n> '
curl -XPOST $URL/$TOKEN -H 'content-type: application/json' -d '{"foo":{"bar":42}}'
printf '\nRetrieving value of data["foo"]["bar"] \n> '
curl $URL/$TOKEN/foo/bar
printf '\nModifing data \n> '
curl -XPUT $URL/$TOKEN/foo -H 'content-type: application/json' -d '{"zoo":"zebra"}'
printf '\nNow data["foo"]["bar"] no longer exists \n> '
curl $URL/$TOKEN/foo/bar
printf '\nDelete data \n> '
curl -XDELETE $URL/$TOKEN
echo
```

Output:
```
Token is: 5e73ba044b45e68b4856925faea268391091f39fc62ab8c58955cf20957018fa
Creating new data 
> {"ok":true}
Retrieving value of data["foo"]["bar"] 
> 42
Modifing data 
> {"ok":true}
Now data["foo"]["bar"] no longer exists 
> {"ok":false}
Delete data 
> {"ok":true}
```