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

import {Actions} from '../common/actions';

import {globals} from './globals';

const ALL_PROCESSES_QUERY = 'select name, pid from process order by name;';

const CPU_TIME_FOR_PROCESSES = `
select
  process.name,
  tot_proc/1e9 as cpu_sec
from
  (select
    upid,
    sum(tot_thd) as tot_proc
  from
    (select
      utid,
      sum(dur) as tot_thd
    from sched group by utid)
  join thread using(utid) group by upid)
join process using(upid)
order by cpu_sec desc limit 100;`;

const CYCLES_PER_P_STATE_PER_CPU = `
select ref as cpu, value as freq, sum(dur * value)/1e6 as mcycles
from counters group by cpu, freq order by mcycles desc limit 20;`;

const CPU_TIME_BY_CLUSTER_BY_PROCESS = `
select
thread.name as comm,
case when cpug = 0 then 'big' else 'little' end as core,
cpu_sec from
  (select cpu/4 cpug, utid, sum(dur)/1e9 as cpu_sec
  from sched group by utid, cpug order by cpu_sec desc)
left join thread using(utid)
limit 20;`;

function createCannedQuery(query: string): (_: Event) => void {
  return (e: Event) => {
    e.preventDefault();
    globals.dispatch(Actions.executeQuery({
      engineId: '0',
      queryId: 'command',
      query,
    }));
  };
}

const EXAMPLE_ANDROID_TRACE_URL =
    'https://storage.googleapis.com/perfetto-misc/example_trace_30s';

const EXAMPLE_CHROME_TRACE_URL =
    'https://storage.googleapis.com/perfetto-misc/example_chrome_trace_10s.json';

const SECTIONS = [
  {
    title: 'Navigation',
    summary: 'Expore a new trace',
    expanded: true,
    items: [
      {t: 'Open trace file', a: popupFileSelectionDialog, i: 'folder_open'},
      {t: 'Record new trace', a: navigateRecord, i: 'fiber_smart_record'},
      {t: 'Show timeline', a: navigateViewer, i: 'fiber_smart_record'},
      {t: 'Share current trace', a: dispatchCreatePermalink, i: 'share'},
    ],
  },
  {
    title: 'Example Traces',
    expanded: true,
    summary: 'Open an example trace',
    items: [
      {
        t: 'Open Android example',
        a: openTraceUrl(EXAMPLE_ANDROID_TRACE_URL),
        i: 'description'
      },
      {
        t: 'Open Chrome example',
        a: openTraceUrl(EXAMPLE_CHROME_TRACE_URL),
        i: 'description'
      },
    ],
  },
  {
    title: 'Metrics and auditors',
    summary: 'Compute summary statistics',
    items: [
      {
        t: 'All Processes',
        a: createCannedQuery(ALL_PROCESSES_QUERY),
        i: 'search',
      },
      {
        t: 'CPU Time by process',
        a: createCannedQuery(CPU_TIME_FOR_PROCESSES),
        i: 'search',
      },
      {
        t: 'Cycles by p-state by CPU',
        a: createCannedQuery(CYCLES_PER_P_STATE_PER_CPU),
        i: 'search',
      },
      {
        t: 'CPU Time by cluster by process',
        a: createCannedQuery(CPU_TIME_BY_CLUSTER_BY_PROCESS),
        i: 'search',
      },
    ],
  },
];

function popupFileSelectionDialog(e: Event) {
  e.preventDefault();
  (document.querySelector('input[type=file]')! as HTMLInputElement).click();
}

function openTraceUrl(url: string): (e: Event) => void {
  return e => {
    e.preventDefault();
    globals.dispatch(Actions.openTraceFromUrl({url}));
  };
}

function onInputElementFileSelectionChanged(e: Event) {
  if (!(e.target instanceof HTMLInputElement)) {
    throw new Error('Not an input element');
  }
  if (!e.target.files) return;
  globals.dispatch(Actions.openTraceFromFile({file: e.target.files[0]}));
}

function navigateRecord(e: Event) {
  e.preventDefault();
  globals.dispatch(Actions.navigate({route: '/record'}));
}

function navigateViewer(e: Event) {
  e.preventDefault();
  globals.dispatch(Actions.navigate({route: '/viewer'}));
}

function dispatchCreatePermalink(e: Event) {
  e.preventDefault();
  // TODO(hjd): Should requestId not be set to nextId++ in the controller?
  globals.dispatch(Actions.createPermalink({
    requestId: new Date().toISOString(),
  }));
}

export class Sidebar implements m.ClassComponent {
  view() {
    const vdomSections = [];
    for (const section of SECTIONS) {
      const vdomItems = [];
      for (const item of section.items) {
        vdomItems.push(
            m('li',
              m(`a[href=#]`,
                {onclick: item.a},
                m('i.material-icons', item.i),
                item.t)));
      }
      vdomSections.push(
          m(`section${section.expanded ? '.expanded' : ''}`,
            m('.section-header',
              {
                onclick: () => {
                  section.expanded = !section.expanded;
                  globals.rafScheduler.scheduleFullRedraw();
                }
              },
              m('h1', section.title),
              m('h2', section.summary), ),
            m('.section-content', m('ul', vdomItems))));
    }
    return m(
        'nav.sidebar',
        m('header', 'Perfetto'),
        m('input[type=file]', {onchange: onInputElementFileSelectionChanged}),
        ...vdomSections);
  }
}
