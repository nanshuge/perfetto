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

import * as m from 'mithril';
import {EngineConfig} from '../common/state';
import {globals} from './globals';
import {MithrilEvent, quietDispatch} from './mithril_helpers';
import {createPage} from './pages';
import {openTrace} from '../common/actions';

const EXAMPLE_TRACE_URL = 'https://storage.googleapis.com/perfetto-misc/example_trace';

function extractFile(e: Event): File|null {
  if (!(e.target instanceof HTMLInputElement)) {
    throw new Error('Not input element');
  }
  if (!e.target.files) return null;
  return e.target.files.item(0);
}

async function loadTraceFromFile(e: MithrilEvent) {
  e.redraw = false;
  const file = extractFile(e);
  if (!file) return;
  const url = await globals.controller.addLocalFile(file);
  globals.dispatch(openTrace(url));
}

const EngineView: m.Component<{engine: EngineConfig}, {}> = {
  view({attrs}) {
    return m('', attrs.engine.id);
  },
};

export const HomePage = createPage({
  view() {
    const engines = Object.values(globals.state.engines);
    return m(
      '.home-page',
      m('.home-page-title', 'Perfetto'),
      m('.home-page-controls',
        m('label.file-input',
          m('input[type=file]', {
            onchange: loadTraceFromFile,
          }),
          'Load trace'),
        ' or ',
        m('button', {
          onclick: quietDispatch(openTrace(EXAMPLE_TRACE_URL)),
        }, 'Open example trace')),
      m('.home-page-traces', engines.map(engine => m(EngineView, {engine}))),
    );
  },
});
