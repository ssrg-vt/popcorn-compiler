build:
	docker run --rm --user $$(id -u):$$(id -g) \
		-v $(PWD)/app:/code/app popcorn-compiler:TransProc \
		make -C /code/app

clean:
	make -C app clean

docker_img:
	docker build -t popcorn-compiler:TransProc -f Dockerfile .
