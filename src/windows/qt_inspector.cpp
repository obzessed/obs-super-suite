#include "qt_inspector.hpp"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QDialog>
#include <QMetaProperty>
#include <QDebug>
#include <QMenuBar>
#include <QStatusBar>
#include <QLayout>

QtInspector::QtInspector(QWidget *parent) : QMainWindow(parent)
{
	setWindowTitle("Qt Widget Inspector");
	resize(800, 600);

	auto *central = new QWidget(this);
	setCentralWidget(central);

	auto *mainLayout = new QVBoxLayout(central);
	
	// Toolbar
	auto *toolbar = new QHBoxLayout();
	auto *refreshBtn = new QPushButton("Refresh", central);
	connect(refreshBtn, &QPushButton::clicked, this, &QtInspector::refreshTree);
	toolbar->addWidget(refreshBtn);

	m_filter = new QLineEdit(central);
	m_filter->setPlaceholderText("Filter by Class/Name...");
	connect(m_filter, &QLineEdit::textChanged, this, &QtInspector::filterTree);
	toolbar->addWidget(m_filter);

	mainLayout->addLayout(toolbar);

	// Splitter for Tree and Properties
	auto *splitter = new QSplitter(Qt::Horizontal, central);
	mainLayout->addWidget(splitter);

	// Tree
	auto *treeContainer = new QWidget(splitter);
	auto *treeLayout = new QVBoxLayout(treeContainer);
	treeLayout->setContentsMargins(0, 0, 0, 0);
	treeLayout->addWidget(new QLabel("Widget Hierarchy:", treeContainer));
	
	m_tree = new QTreeWidget(treeContainer);
	m_tree->setHeaderLabels({"Class", "Name", "Visible", "Pointer"});
	m_tree->setColumnWidth(0, 200);
	m_tree->setColumnWidth(1, 150);
	m_tree->setColumnWidth(2, 60);
	connect(m_tree, &QTreeWidget::itemClicked, this, &QtInspector::onItemSelected);
	treeLayout->addWidget(m_tree);
	splitter->addWidget(treeContainer);

	// Properties
	auto *propsContainer = new QWidget(splitter);
	auto *propsLayout = new QVBoxLayout(propsContainer);
	propsLayout->setContentsMargins(0, 0, 0, 0);
	propsLayout->addWidget(new QLabel("Properties:", propsContainer));

	m_props = new QTableWidget(propsContainer);
	m_props->setColumnCount(2);
	m_props->setHorizontalHeaderLabels({"Property", "Value"});
	m_props->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_props->horizontalHeader()->setStretchLastSection(true);
    connect(m_props, &QTableWidget::itemChanged, this, &QtInspector::onPropertyChanged);
	propsLayout->addWidget(m_props);
	splitter->addWidget(propsContainer);

	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 1);

	refreshTree();
}

QtInspector::~QtInspector()
{
}




void QtInspector::refreshTree()
{
	m_tree->clear();
	m_props->setRowCount(0);
	m_current_widget = nullptr;

	auto allWidgets = QApplication::allWidgets();
    
    QTreeWidgetItem *windowsGroup = new QTreeWidgetItem(m_tree);
    windowsGroup->setText(0, "Windows");
    windowsGroup->setExpanded(true);

    QTreeWidgetItem *dialogsGroup = new QTreeWidgetItem(m_tree);
    dialogsGroup->setText(0, "Dialogs");
    dialogsGroup->setExpanded(true); // Maybe collapse by default if many?

    QTreeWidgetItem *unboundGroup = new QTreeWidgetItem(m_tree);
    unboundGroup->setText(0, "Unbound");
    unboundGroup->setExpanded(true);

    QSet<QWidget*> processed;

    // Pass 1: Promote specific types from ALL widgets
    for (auto *w : allWidgets) {
        if (w == this) continue;

        if (qobject_cast<QMainWindow*>(w)) {
            scanObject(w, windowsGroup);
            processed.insert(w);
        } else if (qobject_cast<QDialog*>(w)) {
            scanObject(w, dialogsGroup);
            processed.insert(w);
        }
    }

    // Pass 2: Catch strict top-levels that weren't promoted
    auto topLevels = QApplication::topLevelWidgets();
    for (auto *w : topLevels) {
        if (w == this) continue;
        if (processed.contains(w)) continue;

        scanObject(w, unboundGroup);
        processed.insert(w);
    }
    
    // Clean up empty groups if desired, or keep them for clarity
    if (windowsGroup->childCount() == 0) windowsGroup->setHidden(true);
    if (dialogsGroup->childCount() == 0) dialogsGroup->setHidden(true);
    if (unboundGroup->childCount() == 0) unboundGroup->setHidden(true);

	// Re-apply filter if needed
	if (!m_filter->text().isEmpty())
		filterTree(m_filter->text());
}

void QtInspector::scanObject(QObject *obj, QTreeWidgetItem *parentItem)
{
	if (!obj) return;

	auto *item = new QTreeWidgetItem();
	
	QString className = obj->metaObject()->className();
    if (qobject_cast<QLayout*>(obj)) className += " [Layout]";
	item->setText(0, className);
	
	QString name = obj->objectName();
    QWidget *w = qobject_cast<QWidget*>(obj);
	if (w && name.isEmpty() && !w->windowTitle().isEmpty())
		name = "[" + w->windowTitle() + "]";

    if (w && parentItem) {
        // Special Roles
        QObject *parentObj = static_cast<QObject*>(parentItem->data(0, Qt::UserRole).value<void*>());
        if (parentObj && parentObj->isWidgetType()) {
            QMainWindow *mw = qobject_cast<QMainWindow*>(parentObj);
            if (mw) {
                if (w == mw->centralWidget()) name += " [CentralWidget]";
                else if (w == static_cast<QWidget*>(mw->menuBar())) name += " [MenuBar]";
                else if (w == mw->statusBar()) name += " [StatusBar]";
            }
        }
    }
	item->setText(1, name);
	
    if (w) item->setText(2, w->isVisible() ? "Yes" : "No");
    else item->setText(2, "-");

	item->setText(3, QString("0x%1").arg((quintptr)obj, 0, 16));
	item->setData(0, Qt::UserRole, QVariant::fromValue((void*)obj));

	if (parentItem)
		parentItem->addChild(item);
	else
		m_tree->addTopLevelItem(item);

	// Children
	auto children = obj->children();
	for (auto *child : children) {
        // Check if child is promoted to root
        if (child->isWidgetType()) {
            if (qobject_cast<QMainWindow*>(child)) continue; // Handled in Windows group
            if (qobject_cast<QDialog*>(child)) continue;     // Handled in Dialogs group
        }

		if (child->isWidgetType() || qobject_cast<QLayout*>(child)) {
			scanObject(child, item);
		}
	}
}

void QtInspector::onItemSelected(QTreeWidgetItem *item, int)
{
	if (!item) return;
	QObject *obj = static_cast<QObject*>(item->data(0, Qt::UserRole).value<void*>());
	if (obj) {
		m_current_widget = obj;
		updateProperties(obj);
	}
}

void QtInspector::updateProperties(QObject *obj)
{
	if (!obj) return;
	
    // Clear old properties
    m_updating = true;
    m_props->setRowCount(0);

	auto addProp = [this](const QString &name, const QString &val, const QString &realPropName = "") {
		int row = m_props->rowCount();
		m_props->insertRow(row);
		auto *keyItem = new QTableWidgetItem(name);
        keyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        // Store actual property name for editing
        keyItem->setData(Qt::UserRole, realPropName.isEmpty() ? name : realPropName);
		m_props->setItem(row, 0, keyItem);
		
        auto *valItem = new QTableWidgetItem(val);
        // Make values editable, except read-only ones
        bool readOnly = realPropName.isEmpty() && !name.startsWith("Window Title") && !name.startsWith("Object Name");
        if (name == "Visible" || name == "Enabled" || name == "StyleSheet") readOnly = false;
        
        if (!readOnly)
            valItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        else
            valItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            
		m_props->setItem(row, 1, valItem);
	};

	const QMetaObject *meta = obj->metaObject();
	addProp("Class", meta->className()); // Read-only
	addProp("Object Name", obj->objectName(), "objectName");

    if (QWidget *w = qobject_cast<QWidget*>(obj)) {
    	addProp("Window Title", w->windowTitle(), "windowTitle");
        QRect g = w->geometry();
        addProp("Geometry", QString("%1, %2 (%3 x %4)").arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height())); // Read only for now
        addProp("Visible", w->isVisible() ? "true" : "false", "visible");
        addProp("Enabled", w->isEnabled() ? "true" : "false", "enabled");
        
        if (w->layout()) {
            addProp("Layout", w->layout()->metaObject()->className());
        }
    	addProp("StyleSheet", w->styleSheet(), "styleSheet");
        
    } else if (QLayout *l = qobject_cast<QLayout*>(obj)) {
        addProp("Layout Items", QString::number(l->count()));
        if (l->parentWidget()) 
            addProp("Parent Widget", l->parentWidget()->objectName().isEmpty() ? 
                l->parentWidget()->metaObject()->className() : l->parentWidget()->objectName());
    }
	
	// Dynamic Properties
	for (int i = 0; i < meta->propertyCount(); i++) {
		QMetaProperty prop = meta->property(i);
		const char *name = prop.name();
        
        // Skip properties we manually handled
        QString sName = QString::fromLatin1(name);
        if (sName == "objectName" || sName == "windowTitle" || sName == "geometry" || 
            sName == "visible" || sName == "enabled" || sName == "styleSheet") continue;
            
		QVariant val = prop.read(obj);
        if (val.isValid())
		    addProp(sName, val.toString(), sName);
	}
    m_updating = false;
}

void QtInspector::onPropertyChanged(QTableWidgetItem *item)
{
    if (m_updating || !m_current_widget || !item) return;
    if (item->column() != 1) return; // Only process value changes

    int row = item->row();
    QTableWidgetItem *keyItem = m_props->item(row, 0);
    QString propName = keyItem->data(Qt::UserRole).toString();
    QString valStr = item->text();

    if (propName.isEmpty()) return;

    m_updating = true;

    // Handle special types
    if (propName == "visible" || propName == "enabled") {
        bool val = (valStr.toLower() == "true" || valStr == "1" || valStr.toLower() == "yes");
        m_current_widget->setProperty(propName.toLatin1(), val);
        item->setText(val ? "true" : "false"); // Normalize
    } else {
        // Try to convert based on property type? 
        QVariant val(valStr);
        // Check meta property to convert type
        int idx = m_current_widget->metaObject()->indexOfProperty(propName.toLatin1());
        if (idx >= 0) {
            QMetaProperty mp = m_current_widget->metaObject()->property(idx);
            if (mp.userType() == QMetaType::Int) val = valStr.toInt();
            else if (mp.userType() == QMetaType::Double) val = valStr.toDouble();
        }
        
        m_current_widget->setProperty(propName.toLatin1(), val);
    }
    
    m_updating = false;
}

void QtInspector::filterTree(const QString &text)
{
    // Helper to set item and all children visibility
    std::function<void(QTreeWidgetItem*, bool)> setVis;
    setVis = [&](QTreeWidgetItem *item, bool vis) {
        item->setHidden(!vis);
        for(int i=0; i<item->childCount(); ++i) {
             // If parent is hidden, children are implicitly hidden in UI but we set state
             setVis(item->child(i), vis);
        }
    };

	if (text.isEmpty()) {
		// Show all
        QTreeWidgetItemIterator it(m_tree);
        while (*it) {
            (*it)->setHidden(false);
            ++it;
        }
		return;
	}

	QString key = text.toLower();
    
    // First hide everything
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        (*it)->setHidden(true);
        ++it;
    }

    // Then reveal matches and their parents
    QTreeWidgetItemIterator it2(m_tree);
    while (*it2) {
        QTreeWidgetItem *item = *it2;
        bool match = item->text(0).toLower().contains(key) || item->text(1).toLower().contains(key);
        
        if (match) {
            item->setHidden(false);
            // Walk up parents
            QTreeWidgetItem *p = item->parent();
            while(p) {
                p->setHidden(false);
                if (!p->isExpanded()) p->setExpanded(true); // Expand path to match
                p = p->parent();
            }
        }
        ++it2;
    }
}
