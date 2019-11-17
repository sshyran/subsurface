// SPDX-License-Identifier: GPL-2.0
#include "mobilelistmodel.h"
#include "filtermodels.h"
#include "core/divelist.h"

MobileListModel::MobileListModel()
	: expandedRow(-1)
{
	connectSignals();
}

MobileListModel *MobileListModel::instance()
{
	static MobileListModel self;
	return &self;
}

void MobileListModel::connectSignals()
{
	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	connect(source, &MultiFilterSortModel::modelAboutToBeReset, this, &MobileListModel::beginResetModel);
	connect(source, &MultiFilterSortModel::modelReset, this, &MobileListModel::endResetModel);
	connect(source, &MultiFilterSortModel::rowsAboutToBeRemoved, this, &MobileListModel::prepareRemove);
	connect(source, &MultiFilterSortModel::rowsRemoved, this, &MobileListModel::doneRemove);
	connect(source, &MultiFilterSortModel::rowsAboutToBeInserted, this, &MobileListModel::prepareInsert);
	connect(source, &MultiFilterSortModel::rowsInserted, this, &MobileListModel::doneInsert);
	connect(source, &MultiFilterSortModel::rowsAboutToBeMoved, this, &MobileListModel::prepareMove);
	connect(source, &MultiFilterSortModel::rowsMoved, this, &MobileListModel::doneMove);
	connect(source, &MultiFilterSortModel::dataChanged, this, &MobileListModel::changed);
}

QHash<int, QByteArray> MobileListModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[DiveTripModelBase::IS_TRIP_ROLE] = "isTrip";
	roles[IsTopLevelRole] = "isTopLevel";
	roles[DiveDateRole] = "date";
	roles[TripIdRole] = "tripId";
	roles[TripNrDivesRole] = "tripNrDives";
	roles[TripShortDateRole] = "tripShortDate";
	roles[TripTitleRole] = "tripTitle";
	roles[DateTimeRole] = "dateTime";
	roles[IdRole] = "id";
	roles[NumberRole] = "number";
	roles[LocationRole] = "location";
	roles[DepthRole] = "depth";
	roles[DurationRole] = "duration";
	roles[DepthDurationRole] = "depthDuration";
	roles[RatingRole] = "rating";
	roles[VizRole] = "viz";
	roles[SuitRole] = "suit";
	roles[AirTempRole] = "airTemp";
	roles[WaterTempRole] = "waterTemp";
	roles[SacRole] = "sac";
	roles[SumWeightRole] = "sumWeight";
	roles[DiveMasterRole] = "diveMaster";
	roles[BuddyRole] = "buddy";
	roles[NotesRole]= "notes";
	roles[GpsRole] = "gps";
	roles[GpsDecimalRole] = "gpsDecimal";
	roles[NoDiveRole] = "noDive";
	roles[DiveSiteRole] = "diveSite";
	roles[CylinderRole] = "cylinder";
	roles[GetCylinderRole] = "getCylinder";
	roles[CylinderListRole] = "cylinderList";
	roles[SingleWeightRole] = "singleWeight";
	roles[StartPressureRole] = "startPressure";
	roles[EndPressureRole] = "endPressure";
	roles[FirstGasRole] = "firstGas";
	roles[SelectedRole] = "selected";
	return roles;
}

// We want to show the newest dives first. Therefore, we have to invert
// the indexes with respect to the source model. To avoid mental gymnastics
// in the rest of the code, we do this right before sending to the
// source model and just after recieving from the source model, respectively.
QModelIndex MobileListModel::sourceIndex(int row, int col, int parentRow) const
{
	if (row < 0 || col < 0)
		return QModelIndex();
	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	QModelIndex parent;
	if (parentRow >= 0) {
		int numTop = source->rowCount(QModelIndex());
		parent = source->index(numTop - 1 - parentRow, 0);
	}
	int numItems = source->rowCount(parent);
	return source->index(numItems - 1 - row, col, parent);
}

int MobileListModel::numSubItems() const
{
	if (expandedRow < 0)
		return 0;
	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	return source->rowCount(sourceIndex(expandedRow, 0));
}

bool MobileListModel::isExpandedRow(const QModelIndex &parent) const
{
	// The main list (parent is invalid) is always expanded.
	if (!parent.isValid())
		return true;

	// A subitem (parent of parent is invalid) is never expanded.
	if (parent.parent().isValid())
		return false;

	return parent.row() == expandedRow;
}

int MobileListModel::invertRow(const QModelIndex &parent, int row) const
{
	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	int numItems = source->rowCount(parent);
	return numItems - 1 - row;
}

int MobileListModel::mapRowFromSourceTopLevel(int row) const
{
	// This is a top-level item. If it is after the expanded row,
	// we have to add the items of the expanded row.
	row = invertRow(QModelIndex(), row);
	return expandedRow >= 0 && row > expandedRow ? row + numSubItems() : row;
}

// The parentRow parameter is the row of the expanded trip converted into
// local "coordinates" as a premature optimization.
int MobileListModel::mapRowFromSourceTrip(const QModelIndex &parent, int parentRow, int row) const
{
	row = invertRow(parent, row);
	if (parentRow != expandedRow) {
		qWarning("MobileListModel::mapRowFromSourceTrip() called on non-extended row");
		return -1;
	}
	return expandedRow + 1 + row; // expandedRow + 1 is the row of the first subitem
}

int MobileListModel::mapRowFromSource(const QModelIndex &parent, int row) const
{
	if (row < 0)
		return -1;

	if (!parent.isValid()) {
		return mapRowFromSourceTopLevel(row);
	} else {
		int parentRow = invertRow(QModelIndex(), parent.row());
		return mapRowFromSourceTrip(parent, parentRow, row);
	}
}

MobileListModel::IndexRange MobileListModel::mapRangeFromSource(const QModelIndex &parent, int first, int last) const
{
	int num = last - first;
	// Since we invert the direction, the last will be the first.
	if (!parent.isValid()) {
		first = mapRowFromSourceTopLevel(last);
		return { QModelIndex(), first, first + num };
	} else {
		int parentRow = invertRow(QModelIndex(), parent.row());
		first = mapRowFromSourceTrip(parent, parentRow, last);
		return { createIndex(parentRow, 0), first, first + num };
	}
}

// This is fun: when inserting, we point to the item *before* which we
// want to insert. But by inverting the direction we turn that into the item
// *after* which we want to insert. Thus, we have to add one to the range.
MobileListModel::IndexRange MobileListModel::mapRangeFromSourceForInsert(const QModelIndex &parent, int first, int last) const
{
	IndexRange res = mapRangeFromSource(parent, first, last);
	++res.first;
	++res.last;
	return res;
}

QModelIndex MobileListModel::mapFromSource(const QModelIndex &idx) const
{
	return createIndex(mapRowFromSource(idx.parent(), idx.row()), idx.column());
}

QModelIndex MobileListModel::mapToSource(const QModelIndex &idx) const
{
	if (!idx.isValid())
		return idx;
	int row = idx.row();
	int col = idx.column();
	if (expandedRow < 0 || row <= expandedRow)
		return sourceIndex(row, col);

	int numSub = numSubItems();
	if (row > expandedRow + numSub)
		return sourceIndex(row - numSub, col);

	return sourceIndex(row - expandedRow - 1, col, expandedRow);
}

QModelIndex MobileListModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	return createIndex(row, column);
}

QModelIndex MobileListModel::parent(const QModelIndex &index) const
{
	// This is a flat model - there is no index
	return QModelIndex();
}

int MobileListModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0; // There is no parent
	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	return source->rowCount() + numSubItems();
}

int MobileListModel::shown()
{
	return shown_dives;
}

int MobileListModel::columnCount(const QModelIndex &parent) const
{
	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	return source->columnCount(parent);
}

QVariant MobileListModel::data(const QModelIndex &index, int role) const
{
	if (role == IsTopLevelRole)
		return index.row() <= expandedRow || index.row() > expandedRow + 1 + numSubItems();

	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	return source->data(mapToSource(index), role);
}

void MobileListModel::resetModel(DiveTripModelBase::Layout layout)
{
	beginResetModel();
	MultiFilterSortModel::instance()->resetModel(layout);
	connectSignals();
	endResetModel();
}

void MobileListModel::prepareRemove(const QModelIndex &parent, int first, int last)
{
	IndexRange range = mapRangeFromSource(parent, first, last);
	if (isExpandedRow(range.parent))
		beginRemoveRows(QModelIndex(), range.first, range.last);
}

void MobileListModel::doneRemove(const QModelIndex &parent, int first, int last)
{
	IndexRange range = mapRangeFromSource(parent, first, last);
	if (isExpandedRow(range.parent)) {
		// Check if we have to move or remove the expanded item
		if (!parent.isValid() && expandedRow >= 0) {
			if (range.first <= expandedRow && range.last >= expandedRow)
				expandedRow = -1;
			else if (range.first <= expandedRow)
				expandedRow -= range.last - range.first + 1;
		}
		endRemoveRows();
	}
}

void MobileListModel::prepareInsert(const QModelIndex &parent, int first, int last)
{
	IndexRange range = mapRangeFromSourceForInsert(parent, first, last);
	if (isExpandedRow(range.parent))
		beginInsertRows(QModelIndex(), range.first, range.last);
}

void MobileListModel::doneInsert(const QModelIndex &parent, int first, int last)
{
	IndexRange range = mapRangeFromSourceForInsert(parent, first, last);
	if (isExpandedRow(range.parent)) {
		// Check if we have to move the expanded item
		if (!parent.isValid() && expandedRow >= 0 && range.first <= expandedRow)
			expandedRow += last - first + 1;
		endInsertRows();
	}
}

// Moving rows is annoying, as there are numerous cases to be considered.
// Some of them degrade to removing or inserting rows.
void MobileListModel::prepareMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow)
{
	IndexRange range = mapRangeFromSource(parent, first, last);
	IndexRange rangeDest = mapRangeFromSourceForInsert(dest, destRow, destRow);
	if (!isExpandedRow(range.parent) && !isExpandedRow(rangeDest.parent))
		return;
	if (isExpandedRow(range.parent) && !isExpandedRow(rangeDest.parent))
		return prepareRemove(parent, first, last);
	if (!isExpandedRow(range.parent) && isExpandedRow(dest))
		return prepareInsert(parent, first, last);
	beginMoveRows(QModelIndex(), range.first, range.last, QModelIndex(), rangeDest.first);
}

void MobileListModel::doneMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow)
{
	IndexRange range = mapRangeFromSource(parent, first, last);
	IndexRange rangeDest = mapRangeFromSourceForInsert(dest, destRow, destRow);
	if (!isExpandedRow(range.parent) && !isExpandedRow(rangeDest.parent))
		return;
	if (isExpandedRow(range.parent) && !isExpandedRow(rangeDest.parent))
		return doneRemove(parent, first, last);
	if (!isExpandedRow(range.parent) && isExpandedRow(dest))
		return doneInsert(parent, first, last);
	if (expandedRow >= 0 && (rangeDest.first < range.first || rangeDest.first > range.last + 1)) {
		if (!parent.isValid() && range.first <= expandedRow && range.last >= expandedRow) {
			// Case 1: the expanded row is in the moved range
			// Since we don't support sub-trips, this means that we can't move into another trip
			if (dest.isValid())
				qWarning("MobileListModel::doneMove(): moving trips into a subtrip");
			else if (rangeDest.first <= range.first)
				expandedRow -=  range.first - rangeDest.first;
			else if (rangeDest.first > range.last + 1)
				expandedRow +=  rangeDest.first - (range.last + 1);
		} else if (range.first > expandedRow && rangeDest.first <= expandedRow) {
			// Case 2: moving things from behind to before the expanded row
			expandedRow += range.last - range.first + 1;
		} else if (range.first < expandedRow && rangeDest.first > expandedRow)  {
			// Case 3: moving things from before to behind the expanded row
			expandedRow -= range.last - range.first + 1;
		}
	}
}

void MobileListModel::expand(int row)
{
	// First, let us treat the trivial cases: expand an invalid row
	// or the row is already expanded.
	if (row < 0) {
		unexpand();
		return;
	}
	if (row == expandedRow)
		return;

	// Collapse the old expanded row, if any.
	if (expandedRow >= 0) {
		int numSub = numSubItems();
		if (row > expandedRow) {
			if (row <= expandedRow + numSub) {
				qWarning("MobileListModel::expand(): trying to expand row in trip");
				return;
			}
			row -= numSub;
		}
		unexpand();
	}

	MultiFilterSortModel *source = MultiFilterSortModel::instance();
	int first = row + 1;
	int last = first + source->rowCount(sourceIndex(row, 0)) - 1;
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
	// We don't support changes beyond levels, sorry.
	if (topLeft.parent().isValid() != bottomRight.parent().isValid()) {
		qWarning("MobileListModel::changed(): changes across different levels. Ignoring.");
		return;
	}

	if (topLeft.parent().isValid()) {
		// This is a range in a trip. First do a sanity check.
		if (topLeft.parent().row() != bottomRight.parent().row()) {
			qWarning("MobileListModel::changed(): changes inside different trips. Ignoring.");
			return;
		}

		// Now check whether this even expanded
		IndexRange range = mapRangeFromSource(topLeft.parent(), topLeft.row(), bottomRight.row());
		if (!isExpandedRow(range.parent))
			return;

		dataChanged(createIndex(range.first, topLeft.column()), createIndex(range.last, bottomRight.column()), roles);
	} else {
		// This is a top-level range.
		IndexRange range = mapRangeFromSource(topLeft.parent(), topLeft.row(), bottomRight.row());

		// If the expanded row is outside the region to be updated
		// or the last entry in the region to be updated, we can simply
		// forward the signal.
		if (expandedRow < 0 || expandedRow < range.first || expandedRow >= range.last) {
			dataChanged(createIndex(range.first, topLeft.column()), createIndex(range.last, bottomRight.column()), roles);
			return;
		}

		// We have to split this in two parts: before and including the expanded row
		// and everything after the expanded row.
		int numSub = numSubItems();
		dataChanged(createIndex(range.first, topLeft.column()), createIndex(expandedRow, bottomRight.column()), roles);
		dataChanged(createIndex(expandedRow + 1 + numSub, topLeft.column()), createIndex(range.last, bottomRight.column()), roles);
	}
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

void MobileListModel::toggle(int row)
{
	if (row < 0)
		return;
	else if (row == expandedRow)
		unexpand();
	else
		expand(row);
}
