# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e
readonly PROJECT_ROOT="$(cd -P ${BASH_SOURCE[0]%/*}/..; pwd)"

case "$(uname -s | tr [:upper:] [:lower:])" in
  darwin)
    readonly OS="mac";;
  linux)
    readonly OS="linux";;
  *)
    echo "Unsupported platform"
    exit 1
    ;;
esac

BIN_PATH="$PROJECT_ROOT/buildtools/$OS/$CMD"
if [ ! -x "$BIN_PATH" ]; then
  echo "Cannot find $BIN_PATH. Run build/install-build-deps first."
  exit 1
fi

exec "$BIN_PATH" "$@"
