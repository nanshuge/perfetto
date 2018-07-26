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

/**
 * A plain js object, holding objects of type |Class| keyed by string id.
 * We use this instead of using |Map| object since it is simpler and faster to
 * serialize for use in postMessage.
 */
export interface ObjectById<Class extends {id: string}> { [id: string]: Class; }

export interface TrackConfig {
  id: string;
  engineId: string;
  type: string;
  height: number;
  name: string;
}

export interface EngineConfig {
  id: string;
  url: string;
}

export interface State {
  route: string|null;
  nextId: number;

  /**
   * Open traces.
   */
  engines: ObjectById<EngineConfig>;


  tracks: ObjectById<TrackConfig>;
}

export function createEmptyState(): State {
  return {
    route: null,
    nextId: 0,
    tracks: {},
    engines: {},
  };
}
