// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import { WasmBridge } from './wasm_bridge';
import { ModuleArgs, Module } from '../gen/trace_processor';

class MockModule implements Module {
  locateFile: (s: string) => string;
  onRuntimeInitialized: () => void;
  onAbort: () => void;
  addFunction = jest.fn();
  ccall = jest.fn();

  constructor() {
    this.locateFile = (_) => {
      throw "locateFile not set";
    };
    this.onRuntimeInitialized = () => {
      throw "onRuntimeInitialized not set";
    };
    this.onAbort = () => {
      throw "onAbort not set";
    };
  }

  init(args: ModuleArgs): Module {
    this.locateFile = args.locateFile;
    this.onRuntimeInitialized = args.onRuntimeInitialized;
    this.onAbort = args.onAbort;
    return this;
  }

  get HEAPU8() {
    const heap = new Uint8Array(10);
    heap.set([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 0);
    return heap;
  }
}

test('wasm bridge should locate files', async () => {
  const m = new MockModule();
  const callback = jest.fn();
  const fileReader = jest.fn();
  new WasmBridge(m.init.bind(m), callback, fileReader);
  expect(m.locateFile('foo.wasm')).toBe('foo.wasm');
});

test('wasm bridge early calls are delayed', async () => {
  const m = new MockModule();
  const callback = jest.fn();
  const fileReader = jest.fn();
  const bridge = new WasmBridge(m.init.bind(m), callback, fileReader);
  
  const requestPromise = bridge.callWasm({
    id: 100,
    serviceName: 'service',
    methodName: 'method',
    data: new Uint8Array(42),
  });

  const readyPromise = bridge.initialize();

  m.onRuntimeInitialized();
  bridge.setBlob(null as any);
  bridge.onReply(0, true, 0, 0);

  await readyPromise;
  bridge.onReply(100, true, 0, 1);
  await requestPromise;
  expect(m.ccall.mock.calls[0][0]).toBe('Initialize');
  expect(m.ccall.mock.calls[1][0]).toBe('service_method');
  expect(callback.mock.calls[0][0].id).toBe(100);
});
