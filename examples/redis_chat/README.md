# Redis example

A simple redis chat server

## Usage

First you need redis running on the port 6379.

### Connect with username

ws://localhost:8080/chat?name=<your-name>

### Enter room

Send message `ENTER <roomNo>` to enter a room.

room number should be 0 - 99.

### Quit room

Send message `QUIT` to quit room.

### Send message

Send whatever else will be a message.