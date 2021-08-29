build:
	docker run --rm -v $(PWD)/app:/code/app popcorn-compiler:main make -C /code/app

clean:
	make -C app clean

docker_img:
	docker build -t popcorn-compiler:main -f Dockerfile .
