# drogon framework example: webserver reverse proxy

__NOTE:__

```
this example show how to use drogon as webserver and using reverse proxy
```

<br>

## steps:

- add these:
    ```txt
    127.0.0.1   localhost.com
    127.0.0.1   api.localhost.com
    127.0.0.1   admin.localhost.com
    127.0.0.1   www.localhost.com
    ```

    to

    - __linux debian__ base location:

        - `/etc/hosts`:

    - __windows__ base location:

        - `C:\Windows\System32\drivers\etc\hosts`:

    ---

- make it sure drogon config is already installed on your machine environment

- build this project, *you can use build-debug-linux.sh on linux environment*

- launch:
    - terminal:
        - go to _*./bin*_ directory and run each of these:
            - `./drogon_server`
            - `./backend_api`
            - `./backend_www`

    - open your browser:
        - go to:
            - `http://www.localhost.com:9999`:
                - this will redirect to `http://localhost.com:9999`

            - `http://localhost.com:9999`:
                - will show example message and which port is called

            - `http://api.localhost.com:9999`:
                - will show json message from which port

            - `http://api.localhost.com:9999/test`:
                - will show json message from `/test` endpoint/path and show which port is called

<br>

---

## preview result example

- [preview result example 1](https://raw.githubusercontent.com/prothegee/prothegee/main/previews/_preview-example-drogon_server-1.png)
- [preview result example 2](https://raw.githubusercontent.com/prothegee/prothegee/main/previews/_preview-example-drogon_server-2.png)

<br>

---

###### end of readme
