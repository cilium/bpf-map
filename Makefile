all: bpf-map

SOURCES := $(shell find . -name '*.go')

bpf-map: bpf-map.go
	go build

clean:
	go clean

install:
	$(INSTALL) -m 0755 -t $(DESTDIR)$(BINDIR) bpf-map
