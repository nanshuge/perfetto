/**
 * Copyright (c) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
 
'use strict';

const GERRIT_REVIEW_URL = 'https://android-review.googlesource.com/c/platform/external/perfetto';
const GERRIT_URL = '/changes/?q=project:platform/external/perfetto+-age:7days&o=DETAILED_ACCOUNTS';
const TRAVIS_URL = 'https://api.travis-ci.org';
const TRAVIS_REPO = 'primiano/perfetto-ci';

function GetTravisStatusForJob(jobId, div) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState != 4 || this.status != 200)
      return;
    var resp = JSON.parse(this.responseText);
    var jobName = resp.config.env.split(' ')[0];
    if (jobName.startsWith('CFG='))
      jobName = jobName.substring(4);
    var link = document.createElement('a');
    link.href = 'https://travis-ci.org/' + TRAVIS_REPO + '/jobs/' + jobId;
    link.innerText = jobName;
    var jobSpan = document.createElement('span');
    jobSpan.appendChild(link);
    jobSpan.classList.add('job');
    jobSpan.classList.add(resp.state);
    div.appendChild(jobSpan);
    console.log(resp);
  };
  xhr.open('GET', TRAVIS_URL + '/jobs/' + jobId, true);
  xhr.send();
}

function GetTravisStatusForCL(clNum, div) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 404) {
      div.innerText = 'Not found';
      return;
    }
    if (this.readyState != 4 || this.status != 200)
      return;
    var resp = JSON.parse(this.responseText);
    for (const jobId of resp.branch.job_ids)
      GetTravisStatusForJob(jobId, div);
  };
  var url = TRAVIS_URL + '/repos/' + TRAVIS_REPO + '/branches/changes/' + clNum;
  xhr.open('GET', url, true);
  xhr.send();
}

function LoadGerritCLs() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState != 4 || this.status != 200)
      return;
    var resp = this.responseText;
    if (resp.startsWith(')]}\''))
      resp = resp.substring(4);
    var resp = JSON.parse(resp);
    var table = document.getElementById('cls');
    for (const cl of resp) {
      var tr = document.createElement('tr');

      var link = document.createElement('a');
      link.href = GERRIT_REVIEW_URL + '/+/' + cl._number;
      link.innerText = cl.subject;
      var td = document.createElement('td');
      td.appendChild(link);
      tr.appendChild(td);

      td = document.createElement('td');
      td.innerText = cl.status;
      tr.appendChild(td);

      td = document.createElement('td');
      td.innerText = cl.owner.email;
      tr.appendChild(td);

      td = document.createElement('td');
      td.innerText = (new Date(cl.updated)).toDateString();
      tr.appendChild(td);

      td = document.createElement('td');
      tr.appendChild(td);
      GetTravisStatusForCL(cl._number, td);

      table.appendChild(tr);
      // console.log(cl);
    }
  };
  xhr.open('GET', GERRIT_URL, true);
  xhr.send();
}

LoadGerritCLs();
