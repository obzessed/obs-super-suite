#include "flow_layout.hpp"
#include <QWidget>

FlowLayout::FlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
	: QLayout(parent),
	  m_hSpace(hSpacing),
	  m_vSpace(vSpacing)
{
	setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing) : m_hSpace(hSpacing), m_vSpace(vSpacing)
{
	setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
	QLayoutItem *item;
	while ((item = takeAt(0)))
		delete item;
}

void FlowLayout::addItem(QLayoutItem *item)
{
	itemList.push_back(item);
}

int FlowLayout::horizontalSpacing() const
{
	if (m_hSpace >= 0) {
		return m_hSpace;
	} else {
		return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
	}
}

int FlowLayout::verticalSpacing() const
{
	if (m_vSpace >= 0) {
		return m_vSpace;
	} else {
		return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
	}
}

int FlowLayout::count() const
{
	return (int)itemList.size();
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
	if (index >= 0 && index < itemList.size())
		return itemList[index];
	return nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
	if (index >= 0 && index < itemList.size()) {
		QLayoutItem *item = itemList[index];
		itemList.erase(itemList.begin() + index);
		return item;
	}
	return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
	return {};
}

bool FlowLayout::hasHeightForWidth() const
{
	return true;
}

int FlowLayout::heightForWidth(int width) const
{
	int height = doLayout(QRect(0, 0, width, 0), true);
	return height;
}

void FlowLayout::setGeometry(const QRect &rect)
{
	QLayout::setGeometry(rect);
	doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
	return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
	QSize size;
	for (const QLayoutItem *item : itemList)
		size = size.expandedTo(item->minimumSize());

	const QMargins margins = contentsMargins();
	size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
	return size;
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
	int left, top, right, bottom;
	getContentsMargins(&left, &top, &right, &bottom);
	QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
	int x = effectiveRect.x();
	int y = effectiveRect.y();
	int lineHeight = 0;

	for (QLayoutItem *item : itemList) {
		if (item->isEmpty())
			continue;

		QWidget *wid = item->widget();
		QSize size = item->sizeHint();

		// If the item has heightForWidth, we should respect it?
		// But for FlowLayout, usually we take the width from sizeHint.
		// If we want "responsive items" that scale, we might need a fixed width per column or something?
		// Here we just use sizeHint().

		if (x + size.width() > effectiveRect.right() && lineHeight > 0) {
			x = effectiveRect.x();
			y = y + lineHeight + verticalSpacing();
			lineHeight = 0;
		}

		if (!testOnly)
			item->setGeometry(QRect(QPoint(x, y), size));

		x = x + size.width() + horizontalSpacing();
		lineHeight = qMax(lineHeight, size.height());
	}
	return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
	QObject *parent = this->parent();
	if (!parent) {
		return -1;
	} else if (parent->isWidgetType()) {
		QWidget *pw = static_cast<QWidget *>(parent);
		return pw->style()->pixelMetric(pm, nullptr, pw);
	} else {
		return static_cast<QLayout *>(parent)->spacing();
	}
}
