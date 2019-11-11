// SPDX-License-Identifier: GPL-2.0
#include "mobilelistmodel.h"

MobileListModel::MobileListModel()
	: expandedRow(-1)
{
	connectSignals();
}

void MobileListModel::connectSignals()
{
	DiveTripModelBase *source = DiveTripModelBase::instance();
	connect(source, &DiveTripModelBase::modelAboutToBeReset, this, &MobileListModel::beginResetModel);
	connect(source, &DiveTripModelBase::modelReset, this, &MobileListModel::endResetModel);
	connect(source, &DiveTripModelBase::rowsAboutToBeRemoved, this, &MobileListModel::prepareRemove);
	connect(source, &DiveTripModelBase::rowsRemoved, this, &MobileListModel::doneRemove);
	connect(source, &DiveTripModelBase::rowsAboutToBeInserted, this, &MobileListModel::prepareInsert);
	connect(source, &DiveTripModelBase::rowsInserted, this, &MobileListModel::doneInsert);
	connect(source, &DiveTripModelBase::rowsAboutToBeMoved, this, &MobileListModel::prepareMove);
	connect(source, &DiveTripModelBase::rowsMoved, this, &MobileListModel::doneMove);
	connect(source, &DiveTripModelBase::dataChanged, this, &MobileListModel::changed);
}

int MobileListModel::numSubItems() const
{
	if (expandedRow < 0)
		return 0;
	DiveTripModelBase *source = DiveTripModelBase::instance();
	return source->rowCount(source->index(expandedRow, 0));
}

bool MobileListModel::isExpandedRow(const QModelIndex &parent) const
{
	// The main list (parent is invalid) is always expanded.
	return !parent.isValid() || parent.row() == expandedRow;
}

int MobileListModel::mapRowFromSource(const QModelIndex &parent, int row) const
{
	if (!parent.isValid()) {
		// This is a top-level item. If it is after the expanded row,
		// we have to add the items of the expanded row.
		return expandedRow >= 0 && row > expandedRow ? row + numSubItems() : row;
	} else {
		// This is a subitem. The function must only be called on
		// expanded subitems.
		int parentRow = parent.row();
		if (parentRow != expandedRow) {
			qWarning("MobileListModel::mapRowFromSource() called on non-extended row");
			return -1;
		}
		return expandedRow + 1 + row; // expandedRow + 1 is the row of the first subitem
	}
}

QModelIndex MobileListModel::mapFromSource(const QModelIndex &idx) const
{
	return index(mapRowFromSource(idx.parent(), idx.row()), idx.column());
}

QVariant MobileListModel::data(const QModelIndex &index, int role) const
{
	switch(role) {
	case IsTopLevelRole:
		return index.row() <= expandedRow || index.row() > expandedRow + 1 + numSubItems();
	case IsTripRole:
		return false; // this is just to make things compile - the new IS_TRIP_ROLE will likely handle this
	}
}

void MobileListModel::resetModel(DiveTripModelBase::Layout layout)
{
	beginResetModel();
	DiveTripModelBase::resetModel(layout);
	connectSignals();
	endResetModel();
}

void MobileListModel::prepareRemove(const QModelIndex &parent, int first, int last)
{
	if (isExpandedRow(parent))
		beginRemoveRows(QModelIndex(), mapRowFromSource(parent, first), mapRowFromSource(parent, last));
}

void MobileListModel::doneRemove(const QModelIndex &parent, int first, int last)
{
	if (isExpandedRow(parent)) {
		// Check if we have to move or remove the expanded item
		if (!parent.isValid() && expandedRow >= 0) {
			if (first <= expandedRow && last >= expandedRow)
				expandedRow = -1;
			else if (first <= expandedRow)
				expandedRow -= last - first + 1;
		}
		endRemoveRows();
	}
}

void MobileListModel::prepareInsert(const QModelIndex &parent, int first, int last)
{
	if (isExpandedRow(parent)) {
		int localRow = mapRowFromSource(parent, first);
		beginInsertRows(QModelIndex(), localRow, localRow + last - first);
	}
}

void MobileListModel::doneInsert(const QModelIndex &parent, int first, int last)
{
	if (isExpandedRow(parent)) {
		// Check if we have to move the expanded item
		if (!parent.isValid() && expandedRow >= 0 && first <= expandedRow)
			expandedRow += last - first + 1;
		endInsertRows();
	}
}

// Moving rows is annoying, as there are numerous cases to be considered.
// Some of them degrade to removing or inserting rows.
void MobileListModel::prepareMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow)
{
	if (!isExpandedRow(parent) && !isExpandedRow(dest))
		return;
	if (isExpandedRow(parent) && !isExpandedRow(dest))
		return prepareRemove(parent, first, last);
	if (!isExpandedRow(parent) && isExpandedRow(dest))
		return prepareInsert(parent, first, last);
	beginMoveRows(QModelIndex(), mapRowFromSource(parent, first), mapRowFromSource(parent, last),
		      QModelIndex(), mapRowFromSource(dest, destRow));
}

void MobileListModel::doneMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow)
{
	if (!isExpandedRow(parent) && !isExpandedRow(dest))
		return;
	if (isExpandedRow(parent) && !isExpandedRow(dest))
		return doneRemove(parent, first, last);
	if (!isExpandedRow(parent) && isExpandedRow(dest))
		return doneInsert(parent, first, last);
	int localFirst = mapRowFromSource(parent, first);
	int localLast = mapRowFromSource(parent, last);
	int localDest = mapRowFromSource(dest, destRow);
	if (expandedRow >= 0 && (localDest < localFirst || localDest > localLast + 1)) {
		if (!parent.isValid() && first <= expandedRow && last >= expandedRow) {
			// Case 1: the expanded row is in the moved range
			// Since we don't support sub-trips, this means that we can't move into another trip
			if (dest.isValid())
				qWarning("MobileListModel::doneMove(): moving trips into a subtrip");
			else if (destRow <= first)
				expandedRow -=  first - destRow;
			else if (destRow > last + 1)
				expandedRow +=  destRow - (last + 1);
		} else if (localFirst > expandedRow && localDest <= expandedRow) {
			// Case 2: moving things from behind to before the expanded row
			expandedRow += localLast - localFirst + 1;
		} else if (localFirst < expandedRow && localDest > expandedRow)  {
			// Case 3: moving things from before to behind the expanded row
			expandedRow -= localLast - localFirst + 1;
		}
	}
}

void MobileListModel::expand(const QModelIndex &index)
{
	// First, let us treat the trivial cases: expand an invalid row
	// or the row is already expanded.
	int row = index.row();
	if (!index.isValid()) {
		unexpand();
		return;
	}
	if (row == expandedRow)
		return;

	// Collapse the old expanded row, if any.
	if (expandedRow > 0) {
		int numSub = numSubItems();
		if (row > expandedRow) {
			if (expandedRow + 1 + numSub > row) {
				qWarning("MobileListModel::expand(): trying to expand row in trip");
				return;
			}
			row -= numSub;
		}
		unexpand();
	}

	DiveTripModelBase *source = DiveTripModelBase::instance();
	int first = row + 1;
	int last = first + source->rowCount(source->index(row, 0)) - 1;
	if (last < first) {
		// Amazingly, Qt's model API doesn't properly handle empty ranges!
		expandedRow = row;
		return;
	}
	beginInsertRows(QModelIndex(), first, last);
	expandedRow = row;
	endInsertRows();
}

void MobileListModel::changed(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
	// If the expanded row is outside the region to be updated
	// or the last entry in the region to be updated, we can simply
	// forward the signal.
	if (expandedRow < 0 || expandedRow < topLeft.row() || expandedRow >= bottomRight.row()) {
		dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
		return;
	}

	// We have to split this in two parts: before and including the expanded row
	// and everything after the expanded row.
	int numSub = numSubItems();
	dataChanged(topLeft, index(expandedRow, bottomRight.column()), roles);
	dataChanged(index(expandedRow + 1 + numSub, topLeft.column()), index(bottomRight.row() + 1 + numSub, bottomRight.column()), roles);
}

void MobileListModel::unexpand()
{
	if (expandedRow < 0)
		return;
	int first = expandedRow + 1;
	int last = first + numSubItems() - 1;
	if (last < first) {
		// Amazingly, Qt's model API doesn't properly handle empty ranges!
		expandedRow = -1;
		return;
	}
	beginRemoveRows(QModelIndex(), first, last);
	expandedRow = -1;
	endRemoveRows();
}

void MobileListModel::flip(const QModelIndex &index)
{
	if (index.row() == expandedRow)
		unexpand();
	else
		expand(index);
}
