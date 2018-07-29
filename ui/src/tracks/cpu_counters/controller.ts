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

import {
  Engine,
  Publish,
  TrackController,
  TrackState
} from '../../controller/track_controller';
import {
  trackControllerRegistry
} from '../../controller/track_controller_registry';

import {TRACK_KIND} from './common';

class CpuCounterTrackController extends TrackController {
  static readonly kind = TRACK_KIND;
  static create(_config: TrackState, _engine: Engine, _publish: Publish):
      TrackController {
    return new CpuCounterTrackController();
  }

  constructor() {
    super();
  }


  onBoundsChange(_start: number, _end: number): void {}
}

trackControllerRegistry.register(CpuCounterTrackController);
