// SPDX-License-Identifier: GPL-2.0
#ifndef MOBILELISTMODEL_H
#define MOBILELISTMODEL_H

#include "divetripmodel.h"

class MobileListModel : public QAbstractItemModel {
	Q_OBJECT
public:
	enum Roles {
		IsTopLevelRole = DiveTripModelBase::LAST_ROLE + 1,
		DiveDateRole,
		TripIdRole,
		TripNrDivesRole,
		TripShortDateRole,
		TripTitleRole,
		DateTimeRole,
		IdRole,
		NumberRole,
		LocationRole,
		DepthRole,
		DurationRole,
		DepthDurationRole,
		RatingRole,
		VizRole,
		SuitRole,
		AirTempRole,
		WaterTempRole,
		SacRole,
		SumWeightRole,
		DiveMasterRole,
		BuddyRole,
		NotesRole,
		GpsDecimalRole,
		GpsRole,
		NoDiveRole,
		DiveSiteRole,
		CylinderRole,
		GetCylinderRole,
		CylinderListRole,
		SingleWeightRole,
		StartPressureRole,
		EndPressureRole,
		FirstGasRole,
		CollapsedRole,
		SelectedRole,
		CurrentRole
	};
	MobileListModel();
	static MobileListModel *instance();
	void resetModel(DiveTripModelBase::Layout layout);	// Switch between tree and list view
	void expand(int row);
	void unexpand();
	void toggle(int row);
private:
	struct IndexRange {
		QModelIndex parent;
		int first, last;
	};
	void connectSignals();
	QModelIndex sourceIndex(int row, int col, int parentRow = -1) const;
	int numSubItems() const;
	bool isExpandedRow(const QModelIndex &parent) const;
	int mapRowFromSourceTopLevel(int row) const;
	int mapRowFromSourceTrip(const QModelIndex &parent, int parentRow, int row) const;
	int mapRowFromSource(const QModelIndex &parent, int row) const;
	int invertRow(const QModelIndex &parent, int row) const;
	IndexRange mapRangeFromSource(const QModelIndex &parent, int first, int last) const;
	IndexRange mapRangeFromSourceForInsert(const QModelIndex &parent, int first, int last) const;
	QModelIndex mapFromSource(const QModelIndex &idx) const;
	QModelIndex mapToSource(const QModelIndex &idx) const;
	static void updateRowAfterRemove(const IndexRange &range, int &row);
	static void updateRowAfterMove(const IndexRange &range, const IndexRange &dest, int &row);
	QVariant data(const QModelIndex &index, int role) const override;
	QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent) const override;
	int columnCount(const QModelIndex &parent) const override;
	QHash<int, QByteArray> roleNames() const override;

	int expandedRow;
	int currentRow; // Row of the currently selected dive, -1 if none.
signals:
	void currentDiveChanged(QModelIndex index);
private slots:
	void prepareRemove(const QModelIndex &parent, int first, int last);
	void doneRemove(const QModelIndex &parent, int first, int last);
	void prepareInsert(const QModelIndex &parent, int first, int last);
	void doneInsert(const QModelIndex &parent, int first, int last);
	void prepareMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow);
	void doneMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow);
	void changed(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
	void currentDiveChangedSlot(QModelIndex index);
};


// Helper functions - these are actually defined in DiveObjectHelper.cpp. Why declare them here?
QString formatSac(const dive *d);
QString formatNotes(const dive *d);
QString format_gps_decimal(const dive *d);
QStringList formatGetCylinder(const dive *d);
QStringList getStartPressure(const dive *d);
QStringList getEndPressure(const dive *d);
QStringList getFirstGas(const dive *d);
QStringList getFullCylinderList();

#endif
