# bpf-map

A small tool to generically introspect BPF maps without requiring to be aware
of the specific data structures stored inside. Can print the metadata of the
map or its contents in hexadecimal form.

## Install

Install from source via `go get`:

```
go get github.com/cilium/bpf-map
```

Download the binary release:

```
curl -SsL https://github.com/cilium/bpf-map/releases/download/v1.0/bpf-map -o bpf-map
chmod +x bpf-map
```

## Usage

```
$ bpf-map dump /sys/fs/bpf/tc/globals/path/map
```

## Example

```
$ sudo bpf-map info /sys/fs/bpf/tc/globals/cilium_lxc
Type:		Hash
Key size:	4
Value size:	104
Max entries:	1024
Flags:		0x0

$ sudo bpf-map dump /sys/fs/bpf/tc/globals/cilium_lxc
Key:
00000000  e8 f7 01 00                                       |....|
Value:
00000000  11 00 00 00 01 02 ca 74  0e c4 d8 25 33 18 00 00  |.......t...%3...|
00000010  1a 19 4b e1 ed 4b 00 00  f0 0d 00 00 00 00 00 00  |..K..K..........|
00000020  c0 a8 21 0b 00 00 74 ca  00 00 00 00 00 00 00 00  |..!...t.........|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000060  00 00 00 00 00 00 00 00                           |........|

Key:
00000000  ee 1c 01 00                                       |....|
Value:
00000000  0f 00 00 00 01 01 d6 8a  4e 9b 9d a5 57 d5 00 00  |........N...W...|
00000010  ba 58 00 80 b9 c1 00 00  f0 0d 00 00 00 00 00 00  |.X..............|
00000020  c0 a8 21 0b 00 00 8a d6  00 00 00 00 00 00 00 00  |..!.............|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000060  00 00 00 00 00 00 00 00                           |........|
[...]
```
