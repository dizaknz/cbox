.PHONY: build
build: main.cpp ## Build
	gcc -g -o tdelta main.cpp -lstdc++	

.PHONY: clean
clean: ## Clean
	rm -rf tdelta

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
