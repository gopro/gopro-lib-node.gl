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

import os.path as op

from PySide6 import QtCore, QtGui, QtWidgets

from pynodegl_utils.misc import Media


class MediasView(QtWidgets.QWidget):

    def __init__(self, config):
        super().__init__()
        self._config = config

        self._model = QtGui.QStandardItemModel()

        self._view = QtWidgets.QListView()
        self._view.setModel(self._model)
        self._view.setAlternatingRowColors(True)
        self._view.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self._view.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self._view.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)

        add_btn = QtWidgets.QPushButton('+')
        rm_btn = QtWidgets.QPushButton('-')
        moveup_btn = QtWidgets.QPushButton('⬆')
        movedown_btn = QtWidgets.QPushButton('⬇')

        btn_layout = QtWidgets.QVBoxLayout()
        btn_layout.addWidget(add_btn)
        btn_layout.addWidget(rm_btn)
        btn_layout.addWidget(moveup_btn)
        btn_layout.addWidget(movedown_btn)
        btn_layout.addStretch()

        layout = QtWidgets.QHBoxLayout()
        layout.addWidget(self._view)
        layout.addLayout(btn_layout)

        self.setLayout(layout)

        add_btn.pressed.connect(self._list_add)
        rm_btn.pressed.connect(self._list_rm)
        moveup_btn.pressed.connect(self._list_moveup)
        movedown_btn.pressed.connect(self._list_movedown)

        self._add_medias(*config.get('medias_list'))

    def _add_medias(self, *paths):
        for path in paths[::-1]:
            try:
                media = Media(path)
            except:
                print(f'Unable to add media {path}')
            else:
                self._config.set_medias_last_dir(op.dirname(path))
                item = QtGui.QStandardItem(path)
                item.setData(media)
                self._model.insertRow(0, item)
        self._update_cfg()

    def _update_cfg(self):
        medias_list = [m.filename for m in self.get_medias()]
        self._config.set_medias_list(medias_list)

    def get_medias(self):
        medias = []
        for row in range(self._model.rowCount()):
            item = self._model.item(row)
            medias.append(item.data())
        return medias

    @QtCore.Slot()
    def _list_add(self):
        last_dir = self._config.get('medias_last_dir')
        filenames = QtWidgets.QFileDialog.getOpenFileNames(self, 'Open media files', last_dir, '')
        self._add_medias(*filenames[0])

    @QtCore.Slot()
    def _list_rm(self):
        selection_model = self._view.selectionModel()
        while True:
            indexes = selection_model.selectedIndexes()
            if not indexes:
                break
            self._model.removeRow(indexes[0].row())
        self._update_cfg()

    @QtCore.Slot()
    def _list_moveup(self):
        selection_model = self._view.selectionModel()
        indexes = selection_model.selectedIndexes()
        row_ids = sorted(index.row() for index in indexes)
        if not row_ids or row_ids[0] == 0:
            return
        for row_id in row_ids:
            row = self._model.takeRow(row_id)
            self._model.insertRow(row_id - 1, row)
            selection_model.select(self._model.indexFromItem(row[0]), QtCore.QItemSelectionModel.Select)
        self._update_cfg()

    @QtCore.Slot()
    def _list_movedown(self):
        selection_model = self._view.selectionModel()
        indexes = selection_model.selectedIndexes()
        last_row = self._model.rowCount() - 1
        row_ids = sorted(index.row() for index in indexes)
        if not row_ids or row_ids[-1] == last_row:
            return
        for row_id in row_ids[::-1]:
            row = self._model.takeRow(row_id)
            self._model.insertRow(row_id + 1, row)
            selection_model.select(self._model.indexFromItem(row[0]), QtCore.QItemSelectionModel.Select)
        self._update_cfg()
