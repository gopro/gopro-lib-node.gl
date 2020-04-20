#!/usr/bin/env python
#
# Copyright 2019 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

from collections import OrderedDict
from PySide2 import QtCore, QtGui, QtWidgets


class HooksView(QtWidgets.QWidget):

    _COLUMNS = ('Session', 'Description', 'Backend', 'System', 'Status')

    def __init__(self, hooks_caller):
        super().__init__()

        self._hooks_caller = hooks_caller

        self._status_column = self._COLUMNS.index('Status')

        self._model = QtGui.QStandardItemModel()

        self._view = QtWidgets.QTableView()
        self._view.setModel(self._model)
        self._view.setAlternatingRowColors(True)
        self._view.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self._view.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self._view.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        self._view.verticalHeader().hide()

        self._refresh_btn = QtWidgets.QPushButton('Refresh')

        hbox = QtWidgets.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self._refresh_btn)

        serial_layout = QtWidgets.QVBoxLayout(self)
        serial_layout.addLayout(hbox)
        serial_layout.addWidget(self._view)

        self._refresh_btn.clicked.connect(self._refresh)

        self._data_cache = OrderedDict()
        self._references = {}
        self._refresh()

    @QtCore.Slot()
    def _refresh(self):
        self._data_cache.update(self.get_data_from_model())

        self._model.clear()
        self._model.setHorizontalHeaderLabels(self._COLUMNS)
        self._references = {}
        for row_id, (session_id, desc, backend, system) in enumerate(self._hooks_caller.get_sessions()):

            # Memorized previous state
            prev_session = self._data_cache.get(session_id, {})
            checked = prev_session.get('checked', True)
            status = prev_session.get('status', '')

            row = [
                QtGui.QStandardItem(session_id),
                QtGui.QStandardItem(desc),
                QtGui.QStandardItem(backend),
                QtGui.QStandardItem(system),
                QtGui.QStandardItem(status),
            ]
            row[0].setCheckable(True)
            row[0].setCheckState(QtCore.Qt.Checked if checked else QtCore.Qt.Unchecked)

            self._model.appendRow(row)
            self._references[session_id] = row_id

        self._view.resizeColumnsToContents()

    def get_data_from_model(self):
        data = OrderedDict()
        for row_id in range(self._model.rowCount()):
            name_item, desc_item, backend_item, system_item, status_item = [self._model.item(row_id, i) for i in range(5)]
            data[name_item.text()] = dict(
                checked=True if name_item.checkState() else False,
                desc=desc_item.text(),
                backend=backend_item.text(),
                system=system_item.text(),
                status=status_item.text(),
            )
        return data

    def update_status(self, id_, status):
        self._model.item(self._references[id_], self._status_column).setText(status)


if __name__ == '__main__':

    import random, string, sys, time


    class DummyHooksCaller:

        '''Fake hooks class generating fake sessions'''

        def __init__(self):
            self._backend = self._random_word()
            self._system = self._random_word()
            self._full_data = [data for data in self._get_random_data()]

        def _get_random_data(self, n=10):
            for row in range(n):
                name = self._random_word()
                desc = self._random_desc()
                yield name, desc, self._backend, self._system

        def _random_word(self, min_length=5, max_length=10):
            return ''.join(random.choice(string.ascii_lowercase) for x in range(random.randint(min_length, max_length))).title()

        def _random_desc(self, min_words=3, max_words=8):
            return ' '.join(self._random_word() for x in range(random.randint(min_words, max_words))).title()

        def get_sessions(self):
            return random.sample(self._full_data, random.randint(2, 8))


    class DummyWindow(QtWidgets.QWidget):

        '''Wrap the HooksView with an additional button to trigger a read of the data + status change'''

        def __init__(self):
            super().__init__()
            self._hooks_view = HooksView(DummyHooksCaller())
            action_btn = QtWidgets.QPushButton('Action!')
            action_btn.clicked.connect(self._do_action)
            layout = QtWidgets.QVBoxLayout()
            layout.addWidget(self._hooks_view)
            layout.addWidget(action_btn)
            self.setLayout(layout)

        @QtCore.Slot()
        def _do_action(self):
            data = self._hooks_view.get_data_from_model()
            for id_, data_row in data.items():
                status = "applied action at {}".format(time.time()) if data_row['checked'] else ''
                self._hooks_view.update_status(id_, status)


    app = QtWidgets.QApplication(sys.argv)
    window = DummyWindow()
    window.show()
    sys.exit(app.exec_())
