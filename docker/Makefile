# Build the application bianary using popcorn (chameleon) compiler
build:
	docker run --rm -v $(PWD)/app:/code/app chameleon:debug make -C /code/app

clean:
	make -C app clean

# Build chameleon docker image
docker_img:
	docker build -t chameleon:debug -f Dockerfile .

run:
	sudo ./chameleon-debug -b ./app/test.blacklist -- ./app/test
