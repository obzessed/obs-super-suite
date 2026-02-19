#pragma once

// ============================================================================
// SMixerNameBar — Channel name display with color accent strip
//
// Shows the channel name with a thin color strip at the top. Features:
//   - Configurable accent color (per-channel identity)
//   - Editable name via double-click
//   - Compact design matching DAW mixer aesthetics
// ============================================================================

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QColor>

namespace super {

class SMixerNameBar : public QWidget {
	Q_OBJECT

public:
	explicit SMixerNameBar(QWidget *parent = nullptr);

	// --- Name ---
	void setName(const QString &name);
	QString name() const;

	// --- Color Strip ---
	void setAccentColor(const QColor &color);
	QColor accentColor() const { return m_accent_color; }

	// --- Configuration ---
	void setEditable(bool editable);
	bool isEditable() const { return m_editable; }

	// --- Editing ---
	void startEditing();

signals:
	void nameChanged(const QString &new_name);
	void doubleClicked();

protected:
	void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
	void onEditFinished();

private:
	void setupUi();
	void finishEditing();

	QWidget *m_color_strip = nullptr;
	QLabel *m_name_label = nullptr;
	QLineEdit *m_name_edit = nullptr;

	QColor m_accent_color{0x00, 0xFA, 0x9A}; // Default: medium spring green
	bool m_editable = true;
	bool m_editing = false;
};

} // namespace super
