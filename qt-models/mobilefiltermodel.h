// SPDX-License-Identifier: GPL-2.0
#ifndef MOBILEFILTERMODEL_H
#define MOBILEFILTERMODEL_H

#include "divetripmodel.h"
#include <QSortFilterProxyModel>

class MobileFilterModel : public QSortFilterProxyModel {
	Q_OBJECT
public:
	static MobileFilterModel *instance();
	bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

	void resetModel(DiveTripModelBase::Layout layout);
	void toggle(int row);
	Q_INVOKABLE int shown(); // number dives that are accepted by the filter
private:
	int mapRowToSource(int row);
	MobileFilterModel();
};

#endif
