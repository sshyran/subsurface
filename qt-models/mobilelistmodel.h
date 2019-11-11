// SPDX-License-Identifier: GPL-2.0
#ifndef MOBILELISTMODEL_H
#define MOBILELISTMODEL_H

#include "divetripmodel.h"

class MobileListModel : public QAbstractItemModel {
	Q_OBJECT
public:
	enum Roles {
		IsTopLevelRole = Qt::UserRole + 1,
		IsTripRole,
		DiveDateRole,
		TripIdRole,
		TripNrDivesRole,
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
	};
	MobileListModel();
	void resetModel(DiveTripModelBase::Layout layout);	// Switch between tree and list view
	void expand(const QModelIndex &index);
	void unexpand();
	void flip(const QModelIndex &index);
private:
	void connectSignals();
	int numSubItems() const;
	bool isExpandedRow(const QModelIndex &parent) const;
	int mapRowFromSource(const QModelIndex &parent, int row) const;
	QModelIndex mapFromSource(const QModelIndex &idx) const;
	QModelIndex mapToSource(const QModelIndex &idx, int role) const;
	QVariant data(const QModelIndex &index, int role) const override;

	int expandedRow;
private slots:
	void prepareRemove(const QModelIndex &parent, int first, int last);
	void doneRemove(const QModelIndex &parent, int first, int last);
	void prepareInsert(const QModelIndex &parent, int first, int last);
	void doneInsert(const QModelIndex &parent, int first, int last);
	void prepareMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow);
	void doneMove(const QModelIndex &parent, int first, int last, const QModelIndex &dest, int destRow);
	void changed(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
};

#endif
