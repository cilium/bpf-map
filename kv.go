//
// Copyright 2016 Authors of Cilium
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
package main

import (
	"encoding/hex"
	"fmt"
	"unsafe"

	"github.com/cilium/cilium/pkg/bpf"
)

// Helper that parses and stores map keys/values
type byteValue struct {
	data []byte

	// Size of related values (so not necessarily the size of the data)
	size uint32
}

func newByteValue(s string, expected, sz uint32) (byteValue, error) {
	b, err := hex.DecodeString(s)
	if err == nil && uint32(len(b)) != expected {
		err = fmt.Errorf("Expected length %v, not %v", expected, len(b))
	}
	return byteValue{b, sz}, err
}

func (b byteValue) NewValue() bpf.MapValue {
	return byteValue{make([]byte, b.size), b.size}
}

func (b byteValue) GetKeyPtr() unsafe.Pointer {
	return b.GetValuePtr()
}

func (b byteValue) GetValuePtr() unsafe.Pointer {
	return unsafe.Pointer(&b.data[0])
}
