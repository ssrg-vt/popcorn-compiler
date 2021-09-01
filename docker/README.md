# Chameleon docker image
Three steps to test chameleon using docker:

1. Build the chameleon:debug docker image
```
make docker_img
```
2. Build the application using this docker image
```
make build
```
3. Run the application under chameleon-debug
```
make run
```

Note: the chameleon building process is broken, so for now, you can only use a
pre-built chameleon binary (i.e., `./chameleon-debug`) to run the application.
