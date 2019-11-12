// SPDX-License-Identifier: GPL-2.0
import QtQuick 2.6
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2
import org.kde.kirigami 2.5 as Kirigami
import org.subsurfacedivelog.mobile 1.0

Kirigami.ScrollablePage {
	id: page
	objectName: "NewDiveList"
	title: qsTr("New Dive list") // this of course needs to be changed
	verticalScrollBarPolicy: Qt.ScrollBarAlwaysOff
	property int credentialStatus: prefs.credentialStatus
	property int numDives: diveListView.count
	property color textColor: subsurfaceTheme.textColor
	property color secondaryTextColor: subsurfaceTheme.secondaryTextColor
	property int horizontalPadding: Kirigami.Units.gridUnit / 2 - Kirigami.Units.smallSpacing  + 1
	property QtObject diveListModel: mobileListModel
	property string numShownText

	supportsRefreshing: true
	onRefreshingChanged: {
		if (refreshing) {
			if (prefs.credentialStatus === CloudStatus.CS_VERIFIED) {
				detailsWindow.endEditMode()
				manager.saveChangesCloud(true)
				refreshing = false
			} else {
				manager.appendTextToLog("sync with cloud storage requested, but credentialStatus is " + prefs.credentialStatus)
				manager.appendTextToLog("no syncing, turn off spinner")
				refreshing = false
			}
		}
	}

	Component {
		id: diveOrTripDelegate
		Kirigami.AbstractListItem {
			// this allows us to access properties of the currentItem from outside
			property variant myData: model
			id: diveOrTripDelegateItem
			leftPadding: 0
			topPadding: 0
			supportsMouseEvents: true
			checked: !model.isTrip && model.selected
			anchors {
				left: parent.left
				right: parent.right
			}

			height: Kirigami.Units.gridUnit * 2 + Kirigami.Units.smallSpacing
			backgroundColor: checked ? subsurfaceTheme.primaryColor : subsurfaceTheme.backgroundColor
			activeBackgroundColor: subsurfaceTheme.primaryColor
			textColor: checked ? subsurfaceTheme.primaryTextColor : subsurfaceTheme.textColor

			// When clicked, a trip expands / unexpands, a dive is opened in DiveDetails
			onClicked: {
				if (model.isTrip) {
					manager.appendTextToLog("clicked on trip " + model.tripTitle)
					manager.toggle(model.row);
					// toggle expand (backend to deal with unexpand other trip)
				} else {
					manager.appendTextToLog("clicked on dive")
					if (detailsWindow.state === "view") {
						detailsWindow.showDiveIndex(id); // we need access to dive->id
						// switch to detailsWindow (or push it if it's not in the stack)
						var i = rootItem.pageIndex(detailsWindow)
						if (i === -1)
							pageStack.push(detailsWindow)
						else
							pageStack.currentIndex = i
					}
				}
			}

			// first we look at the trip
			Item {
				width: page.width
				height: childrenRect.height
				Rectangle {
					id: headingBackground
					height: sectionText.height + Kirigami.Units.gridUnit
					anchors {
						left: parent.left
						right: parent.right
					}
					color: subsurfaceTheme.lightPrimaryColor
					visible: isTrip
					Rectangle {
						id: dateBox
						height: parent.height - Kirigami.Units.smallSpacing
						width: 2.5 * Kirigami.Units.gridUnit * PrefDisplay.mobile_scale
						color: subsurfaceTheme.primaryColor
						radius: Kirigami.Units.smallSpacing * 2
						antialiasing: true
						anchors {
							verticalCenter: parent.verticalCenter
							left: parent.left
							leftMargin: Kirigami.Units.smallSpacing
						}
						Controls.Label {
							text: tripShortDate
							color: subsurfaceTheme.primaryTextColor
							font.pointSize: subsurfaceTheme.smallPointSize
							lineHeightMode: Text.FixedHeight
							lineHeight: Kirigami.Units.gridUnit *.9
							horizontalAlignment: Text.AlignHCenter
							anchors {
								horizontalCenter: parent.horizontalCenter
								verticalCenter: parent.verticalCenter
							}
						}
					}
					Controls.Label {
						id: sectionText
						text: tripTitle
						wrapMode: Text.WrapAtWordBoundaryOrAnywhere
						visible: text !== ""
						font.weight: Font.Bold
						font.pointSize: subsurfaceTheme.regularPointSize
						anchors {
							top: parent.top
							left: dateBox.right
							topMargin: Math.max(2, Kirigami.Units.gridUnit / 2)
							leftMargin: horizontalPadding * 2
							right: parent.right
						}
						color: subsurfaceTheme.lightPrimaryTextColor
					}
				}
				Rectangle {
					height: section == "" ? 0 : 1
					width: parent.width
					anchors.top: headingBackground.bottom
					color: "#B2B2B2"
				}
			}
		}
	}

	StartPage {
		id: startPage
		anchors.fill: parent
		opacity: (credentialStatus === CloudStatus.CS_NOCLOUD ||
			 credentialStatus === CloudStatus.CS_VERIFIED) ? 0 : 1
		visible: opacity > 0
		Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
		function setupActions() {
			if (prefs.credentialStatus === CloudStatus.CS_VERIFIED ||
					prefs.credentialStatus === CloudStatus.CS_NOCLOUD) {
				page.actions.main = page.downloadFromDCAction
				page.actions.right = page.addDiveAction
				page.actions.left = page.filterToggleAction
				page.title = qsTr("Dive list")
				if (diveListView.count === 0)
					showPassiveNotification(qsTr("Please tap the '+' button to add a dive (or download dives from a supported dive computer)"), 3000)
			} else {
				page.actions.main = null
				page.actions.right = null
				page.actions.left = null
				page.title = qsTr("Cloud credentials")
			}
		}
		onVisibleChanged: {
			setupActions();
		}

		Component.onCompleted: {
			manager.finishSetup();
			setupActions();
		}
	}

	Controls.Label {
		anchors.fill: parent
		horizontalAlignment: Text.AlignHCenter
		verticalAlignment: Text.AlignVCenter
		text: diveListModel ? qsTr("No dives in dive list") : qsTr("Please wait, filtering dive list")
		visible: diveListView.visible && diveListView.count === 0
	}

	// FIXME: deleted all of the filter handling

	ListView {
		id: diveListView
		anchors.fill: parent
		opacity: 1.0 - startPage.opacity
		visible: opacity > 0
		model: diveListModel
		currentIndex: -1
		delegate: diveOrTripDelegate
		// header: filterHeader
		// headerPositioning: ListView.OverlayHeader
		boundsBehavior: Flickable.DragOverBounds
		maximumFlickVelocity: parent.height * 5
		bottomMargin: Kirigami.Units.iconSizes.medium + Kirigami.Units.gridUnit
		cacheBuffer: 40 // this will increase memory use, but should help with scrolling
		onModelChanged: {
			numShownText = mobileListModel.shown()
		}
		Component.onCompleted: {
			manager.appendTextToLog("finished setting up the new diveListView")
		}
	}

	function showDownloadPage(vendor, product, connection) {
		downloadFromDc.dcImportModel.clearTable()
		pageStack.push(downloadFromDc)
		if (vendor !== undefined && product !== undefined && connection !== undefined) {
			/* set up the correct values on the download page */
			if (vendor !== -1)
				downloadFromDc.vendor = vendor
			if (product !== -1)
				downloadFromDc.product = product
			if (connection !== -1)
				downloadFromDc.connection = connection
		}
	}

	property QtObject downloadFromDCAction: Kirigami.Action {
		icon {
			name: ":/icons/downloadDC"
			color: subsurfaceTheme.primaryColor
		}
		text: qsTr("Download dives")
		onTriggered: {
			showDownloadPage()
		}
	}

	property QtObject addDiveAction: Kirigami.Action {
		icon {
			name: ":/icons/list-add"
		}
		text: qsTr("Add dive")
		onTriggered: {
			startAddDive()
		}
	}

	property QtObject filterToggleAction: Kirigami.Action {
		icon {
			name: ":icons/ic_filter_list"
		}
		text: qsTr("Filter dives")
		onTriggered: {
			rootItem.filterToggle = !rootItem.filterToggle
			manager.setFilter("")
			numShownText = diveModel.shown()
		}
	}

	onBackRequested: {
		if (startPage.visible && diveListView.count > 0 &&
			prefs.credentialStatus !== CloudStatus.CS_INCORRECT_USER_PASSWD) {
			prefs.credentialStatus = oldStatus
			event.accepted = true;
		}
		if (!startPage.visible) {
			if (Qt.platform.os != "ios") {
				manager.quit()
			}
			// let's make sure Kirigami doesn't quit on our behalf
			event.accepted = true
		}
	}

	function setCurrentDiveListIndex(idx, noScroll) {
		// pick the dive in the dive list and make sure its trip is expanded
		diveListView.currentIndex = idx
		if (diveListModel)
			diveListModel.setActiveTrip(diveListView.currentItem.myData.tripId)

		// update the diveDetails page to also show that dive
		detailsWindow.showDiveIndex(idx)

		// updating the index of the ListView triggers a non-linear scroll
		// animation that can be very slow. the fix is to stop this animation
		// by setting contentY to itself and then using positionViewAtIndex().
		// the downside is that the view jumps to the index immediately.
		if (noScroll) {
			diveListView.contentY = diveListView.contentY
			diveListView.positionViewAtIndex(idx, ListView.Center)
		}
	}
}
