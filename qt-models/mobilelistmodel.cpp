// SPDX-License-Identifier: GPL-2.0
#include "mobilelistmodel.h"
#include "divetripmodel.h"

MobileListModelBase::MobileListModelBase(DiveTripModelBase *sourceIn) : source(sourceIn)
{
}

QHash<int, QByteArray> MobileListModelBase::roleNames() const
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
	roles[CurrentRole] = "current";
	return roles;
}

int MobileListModelBase::columnCount(const QModelIndex &parent) const
{
	return source->columnCount(parent);
}

QModelIndex MobileListModelBase::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	return createIndex(row, column);
}

QModelIndex MobileListModelBase::parent(const QModelIndex &index) const
{
	// These are flat models - there is no parent
	return QModelIndex();
}

MobileListModel::MobileListModel(DiveTripModelBase *source) : MobileListModelBase(source),
	expandedRow(-1),
	currentRow(-1)
{
	connect(source, &DiveTripModelBase::modelAboutToBeReset, this, &MobileListModel::beginResetModel);
	connect(source, &DiveTripModelBase::modelReset, this, &MobileListModel::endResetModel);
	connect(source, &DiveTripModelBase::rowsAboutToBeRemoved, this, &MobileListModel::prepareRemove);
	connect(source, &DiveTripModelBase::rowsRemoved, this, &MobileListModel::doneRemove);
	connect(source, &DiveTripModelBase::rowsAboutToBeInserted, this, &MobileListModel::prepareInsert);
	connect(source, &DiveTripModelBase::rowsInserted, this, &MobileListModel::doneInsert);
	connect(source, &DiveTripModelBase::rowsAboutToBeMoved, this, &MobileListModel::prepareMove);
	connect(source, &DiveTripModelBase::rowsMoved, this, &MobileListModel::doneMove);
	connect(source, &DiveTripModelBase::dataChanged, this, &MobileListModel::changed);
	connect(source, &DiveTripModelBase::currentDiveChanged, this, &MobileListModel::currentDiveChangedSlot);
}

// We want to show the newest dives first. Therefore, we have to invert
// the indexes with respect to the source model. To avoid mental gymnastics
// in the rest of the code, we do this right before sending to the
// source model and just after recieving from the source model, respectively.
QModelIndex MobileListModel::sourceIndex(int row, int col, int parentRow) const
{
	if (row < 0 || col < 0)
		return QModelIndex();
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

int MobileListModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0; // There is no parent
	return source->rowCount() + numSubItems();
}

QVariant MobileListModel::data(const QModelIndex &index, int role) const
{
	if (role == IsTopLevelRole)
		return index.row() <= expandedRow || index.row() > expandedRow + 1 + numSubItems();
	else if (role == CurrentRole)
		return index.row() == currentRow;

	return source->data(mapToSource(index), role);
}

void MobileListModel::prepareRemove(const QModelIndex &parent, int first, int last)
{
	IndexRange range = mapRangeFromSource(parent, first, last);
	if (isExpandedRow(range.parent))
		beginRemoveRows(QModelIndex(), range.first, range.last);
}

void MobileListModel::updateRowAfterRemove(const IndexRange &range, int &row)
{
	if (row < 0)
		return;
	else if (range.first <= row && range.last >= row)
		row = -1;
	else if (range.first <= row)
		row -= range.last - range.first + 1;
}

void MobileListModel::doneRemove(const QModelIndex &parent, int first, int last)
{
	IndexRange range = mapRangeFromSource(parent, first, last);
	if (isExpandedRow(range.parent)) {
		// Check if we have to move or remove the expanded or current item
		updateRowAfterRemove(range, expandedRow);
		updateRowAfterRemove(range, currentRow);

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
		// Check if we have to move the current item
		if (currentRow >= 0 && range.first <= currentRow)
			currentRow += last - first + 1;
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

void MobileListModel::updateRowAfterMove(const IndexRange &range, const IndexRange &rangeDest, int &row)
{
	if (row >= 0 && (rangeDest.first < range.first || rangeDest.first > range.last + 1)) {
		if (range.first <= row && range.last >= row) {
			// Case 1: the expanded row is in the moved range
			if (rangeDest.first <= range.first)
				row -=  range.first - rangeDest.first;
			else if (rangeDest.first > range.last + 1)
				row +=  rangeDest.first - (range.last + 1);
		} else if (range.first > row && rangeDest.first <= row) {
			// Case 2: moving things from behind to before the expanded row
			row += range.last - range.first + 1;
		} else if (range.first < row && rangeDest.first > row)  {
			// Case 3: moving things from before to behind the expanded row
			row -= range.last - range.first + 1;
		}
	}
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
	updateRowAfterMove(range, rangeDest, expandedRow);
	updateRowAfterMove(range, rangeDest, currentRow);
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
	currentRow = -1; // We collapsed all visible dives -> there is no visible current dive.
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

void MobileListModel::currentDiveChangedSlot(QModelIndex index)
{
	// If this is in a trip, expand the trip first,
	// potentially removing the old current dive.
	if (index.parent().isValid()) {
		int row = mapRowFromSourceTopLevel(index.parent().row());
		expand(row);
	} else {
		// If outside of a trip, collapse any expanded trip.
		unexpand();
	}

	int row = index.isValid() ? mapRowFromSource(index.parent(), index.row()) : -1;
	if (row == currentRow)
		return; // No change.

	static const QVector<int> roles = { CurrentRole };
	QModelIndex oldIdx = createIndex(currentRow, 0);
	QModelIndex newIdx = row >= 0 ? createIndex(row, 0) : QModelIndex();
	currentRow = row;
	dataChanged(oldIdx, oldIdx, roles);
	if (row >= 0)
		dataChanged(newIdx, newIdx, roles);
	emit currentDiveChanged(newIdx);
}

MobileSwipeModel::MobileSwipeModel(DiveTripModelBase *source) : MobileListModelBase(source)
{
	connect(source, &DiveTripModelBase::modelAboutToBeReset, this, &MobileSwipeModel::beginResetModel);
	connect(source, &DiveTripModelBase::modelReset, this, &MobileSwipeModel::doneReset);
	connect(source, &DiveTripModelBase::rowsAboutToBeRemoved, this, &MobileSwipeModel::prepareRemove);
	connect(source, &DiveTripModelBase::rowsRemoved, this, &MobileSwipeModel::doneRemove);
	connect(source, &DiveTripModelBase::rowsAboutToBeInserted, this, &MobileSwipeModel::prepareInsert);
	connect(source, &DiveTripModelBase::rowsInserted, this, &MobileSwipeModel::doneInsert);
	connect(source, &DiveTripModelBase::rowsAboutToBeMoved, this, &MobileSwipeModel::prepareMove);
	connect(source, &DiveTripModelBase::rowsMoved, this, &MobileSwipeModel::doneMove);
	connect(source, &DiveTripModelBase::dataChanged, this, &MobileSwipeModel::changed);
	connect(source, &DiveTripModelBase::currentDiveChanged, this, &MobileSwipeModel::currentDiveChangedSlot);

	initData();
}

void MobileSwipeModel::initData()
{
	rows = 0;
	int act = 0;
	int topLevelRows = source->rowCount();
	firstElement.resize(topLevelRows);
	for (int i = 0; i < topLevelRows; ++i) {
		firstElement[i] = act;
		// Note: we populate the model in reverse order, because we show the newest dives first.
		QModelIndex index = source->index(topLevelRows - i - 1, 0, QModelIndex());
		act += source->data(index, DiveTripModelBase::IS_TRIP_ROLE).value<bool>() ?
			source->rowCount(index) : 1;
	}
	rows = act;
	invalidateSourceRowCache();
}

void MobileSwipeModel::doneReset()
{
	initData();
	endResetModel();
}

void MobileSwipeModel::invalidateSourceRowCache() const
{
	cachedRow = -1;
	cacheSourceParent = QModelIndex();
	cacheSourceRow = -1;
}

void MobileSwipeModel::updateSourceRowCache(int localRow) const
{
	if (firstElement.empty())
		return invalidateSourceRowCache();

	cachedRow = localRow;

	// Do a binary search for the first top-level item that starts after the given row
	auto idx = std::upper_bound(firstElement.begin(), firstElement.end(), localRow);
	if (idx == firstElement.begin())
		return invalidateSourceRowCache(); // Huh? localRow was negative? Then index->isValid() should have returned true.

	--idx;
	int topLevelRow = idx - firstElement.begin();
	int topLevelRowSource = firstElement.end() - idx - 1; // Reverse direction.
	int indexInRow = localRow - *idx;
	if (indexInRow == 0) {
		// This might be a top-level dive or a one-dive trip. Perhaps we should save which one it is.
		if (!source->data(source->index(topLevelRowSource, 0), DiveTripModelBase::IS_TRIP_ROLE).value<bool>()) {
			cacheSourceParent = QModelIndex();
			cacheSourceRow = topLevelRowSource;
			return;
		}
	}
	cacheSourceParent = source->index(topLevelRowSource, 0);
	int numElements = elementCountInTopLevel(topLevelRow);
	cacheSourceRow = numElements - indexInRow - 1;
}

QModelIndex MobileSwipeModel::mapToSource(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();
	if (index.row() != cachedRow)
		updateSourceRowCache(index.row());

	return cacheSourceRow >= 0 ? source->index(cacheSourceRow, index.column(), cacheSourceParent) : QModelIndex();
}

int MobileSwipeModel::mapTopLevelFromSource(int row) const
{
	return firstElement.size() - row - 1;
}

int MobileSwipeModel::elementCountInTopLevel(int row) const
{
	if (row < 0 || row >= (int)firstElement.size())
		return 0;
	if (row + 1 < (int)firstElement.size())
		return firstElement[row + 1] - firstElement[row];
	else
		return rows - firstElement[row];
}

int MobileSwipeModel::mapRowFromSource(const QModelIndex &parent, int row) const
{
	if (parent.isValid()) {
		int topLevelRow = mapTopLevelFromSource(parent.row());
		int count = elementCountInTopLevel(topLevelRow);
		return firstElement[topLevelRow] + count - row - 1; // Note: we invert the direction!
	} else {
		int topLevelRow = mapTopLevelFromSource(row);
		return firstElement[topLevelRow];
	}
}

int MobileSwipeModel::mapRowFromSource(const QModelIndex &idx) const
{
	return mapRowFromSource(idx.parent(), idx.row());
}

// Remove top-level items. Parameters with standard range semantics (pointer to first and past last element).
int MobileSwipeModel::removeTopLevel(int begin, int end)
{
	auto it1 = firstElement.begin() + begin;
	auto it2 = firstElement.begin() + end;
	int count = std::accumulate(it1, it2, 0); // Number of items we have to subtract from rest
	firstElement.erase(it1, it2); // Remove items
	for (auto act = firstElement.begin() + begin; act != firstElement.end(); ++act)
		*act -= count; // Subtract removed items
	return count;
}

// Add or remove subitems from top-level items
void MobileSwipeModel::updateTopLevel(int row, int delta)
{
	for (int i = row; i < (int)firstElement.size(); ++i)
		firstElement[i] += delta;
}

// Add items at top-level. The number of subelements of each items is given in the second parameter.
void MobileSwipeModel::addTopLevel(int row, const std::vector<int> &items)
{
	int count = std::accumulate(items.begin(), items.end(), 0); // Number of items we are going to add
	auto it = firstElement.begin() + row;
	for (auto act = it; act != firstElement.end(); ++act)
		*act += count;
	firstElement.insert(it, items.begin(), items.end());
}

void MobileSwipeModel::prepareRemove(const QModelIndex &parent, int first, int last)
{
	// Remember to invert the direction.
	beginRemoveRows(QModelIndex(), mapRowFromSource(parent, last), mapRowFromSource(parent, first));
}

void MobileSwipeModel::doneRemove(const QModelIndex &parent, int first, int last)
{
	if (!parent.isValid()) {
		// This is a top-level range. This means that we have to remove top-level items.
		// Remember to invert the direction.
		removeTopLevel(mapTopLevelFromSource(last), mapTopLevelFromSource(first) + 1);
	} else {
		// This is part of a trip. Only the number of items has to be changed.
		updateTopLevel(mapTopLevelFromSource(parent.row()), -(last - first + 1));
	}
	invalidateSourceRowCache();
	endRemoveRows();
}

void MobileSwipeModel::prepareInsert(const QModelIndex &parent, int first, int last)
{
	// We can not call beginInsertRows here, because before the source model
	// has inserted its rows we don't know how many subitems there are!
}

void MobileSwipeModel::doneInsert(const QModelIndex &parent, int first, int last)
{
	if (!parent.isValid()) {
		// This is a top-level range. This means that we have to add top-level items.

		// Create vector of new top-level items
		std::vector<int> items;
		items.reserve(last - first + 1);
		int count = 0;
		for (int row = last; row <= first; --row) {
			items.push_back(source->rowCount(source->index(row, 0)));
			count += items.back();
		}

		int firstLocal = mapTopLevelFromSource(first);
		beginInsertRows(QModelIndex(), firstLocal, firstLocal + count - 1);
		addTopLevel(mapTopLevelFromSource(first), items);
		endInsertRows();
	} else {
		// This is part of a trip. Only the number of items has to be changed.
		beginInsertRows(QModelIndex(), mapRowFromSource(parent, last), mapRowFromSource(parent, first));
		updateTopLevel(mapTopLevelFromSource(parent.row()), last - first + 1);
		endInsertRows();
	}
	invalidateSourceRowCache();
}

void MobileSwipeModel::prepareMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow)
{
	beginMoveRows(QModelIndex(), mapRowFromSource(parent, last), mapRowFromSource(parent, first), QModelIndex(), mapRowFromSource(dest, destRow));
}

void MobileSwipeModel::doneMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow)
{
	// Moving is annoying. There are four cases to consider, depending whether
	// we move in / out of a top-level item!
	if (!parent.isValid() && !dest.isValid()) {
		// From top-level to top-level
		if (destRow < first || destRow > last + 1) {
			int beginLocal = mapTopLevelFromSource(last);
			int endLocal = mapTopLevelFromSource(first) + 1;
			int destLocal = mapTopLevelFromSource(destRow);
			int count = endLocal - beginLocal;
			std::vector<int> items;
			items.reserve(count);
			for (int row = beginLocal; row < endLocal; ++row) {
				items.push_back(row < (int)firstElement.size() - 1 ? firstElement[row + 1] - firstElement[row]
									      : rows - firstElement[row]);
			}
			removeTopLevel(mapTopLevelFromSource(last), mapTopLevelFromSource(first) + 1);

			if (destLocal >= beginLocal)
				destLocal -= count;
			addTopLevel(destLocal, items);
		}
	} else if (!parent.isValid() && dest.isValid()) {
		// From top-level to trip
		int beginLocal = mapTopLevelFromSource(last);
		int endLocal = mapTopLevelFromSource(first) + 1;
		int destLocal = mapTopLevelFromSource(dest.row());
		int count = endLocal - beginLocal;
		int numMoved = removeTopLevel(beginLocal, endLocal);
		if (destLocal >= beginLocal)
			destLocal -= count;
		updateTopLevel(destLocal, numMoved);
	} else if (parent.isValid() && !dest.isValid()) {
		// From trip to top-level
		int fromLocal = mapTopLevelFromSource(parent.row());
		int toLocal = mapTopLevelFromSource(dest.row());
		int numMoved = last - first + 1;
		std::vector<int> items(numMoved, 1); // This can only be dives -> item count is 1
		updateTopLevel(fromLocal, -numMoved);
		addTopLevel(toLocal, items);
	} else {
		// From trip to other trip
		int fromLocal = mapTopLevelFromSource(parent.row());
		int toLocal = mapTopLevelFromSource(dest.row());
		int numMoved = last - first + 1;
		if (fromLocal != toLocal) {
			updateTopLevel(fromLocal, -numMoved);
			updateTopLevel(toLocal, numMoved);
		}
	}
	invalidateSourceRowCache();
}

void MobileSwipeModel::changed(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
	if (!topLeft.isValid() || !bottomRight.isValid())
		return;
	int fromSource = mapRowFromSource(bottomRight);
	int toSource = mapRowFromSource(topLeft);
	dataChanged(createIndex(fromSource, topLeft.column()), createIndex(toSource, bottomRight.column()), roles);
}

void MobileSwipeModel::currentDiveChangedSlot(QModelIndex index)
{
	if (!index.isValid()) {
		emit currentDiveChanged(QModelIndex());
		return;
	}
	int rowSource = mapRowFromSource(index);
	emit currentDiveChanged(createIndex(rowSource, 0));
}

QVariant MobileSwipeModel::data(const QModelIndex &index, int role) const
{
	return source->data(mapToSource(index), role);
}

int MobileSwipeModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0; // There is no parent
	return rows;
}

MobileModels *MobileModels::instance()
{
	static MobileModels self;
	return &self;
}

MobileModels::MobileModels() :
	lm(&source),
	sm(&source)
{
	reset();
}

MobileListModel *MobileModels::listModel()
{
	return &lm;
}

MobileSwipeModel *MobileModels::swipeModel()
{
	return &sm;
}

void MobileModels::clear()
{
	source.clear();
}

void MobileModels::reset()
{
	source.reset();
}
