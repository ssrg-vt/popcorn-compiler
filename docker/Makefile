all: basic pingpong

basic:
	docker run --rm \
		-v $(PWD)/basic:/code/app \
		--user "$$(id -u):$$(id -g)" \
		popcorn-compiler:main make -C /code/app

pingpong:
	docker run --rm \
		-v $(PWD)/pingpong:/code/app \
		--user "$$(id -u):$$(id -g)" \
		popcorn-compiler:main make -C /code/app

clean:
	make -C basic clean
	make -C pingpong clean

docker_img:
	docker build -t popcorn-compiler:main -f Dockerfile .

.PHONY: all basic pingpong clean
