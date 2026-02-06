#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSet>

struct AsioSourceConfig;

class AsioSourceDialog : public QDialog {
	Q_OBJECT

public:
	enum Mode { AddMode, EditMode };

	explicit AsioSourceDialog(Mode mode, QWidget *parent = nullptr);
	~AsioSourceDialog() = default;

	// Set existing config for edit mode
	void setConfig(const AsioSourceConfig &cfg);
	
	// Get the configured values
	QString getName() const;
	QString getSourceType() const;
	QString getCanvas() const;
	int getChannel() const;
	bool shouldOpenProperties() const;
	void setOpenProperties(bool open);
	bool shouldStartMuted() const;
	void setStartMuted(bool muted);

	// For edit mode: the channel/canvas being edited (always enabled)
	void setCurrentChannel(int channel);

private slots:
	void validateInput();
	void onCanvasChanged();

private:
	void setupUi();
	void populateCanvases();
	void populateChannels();

	Mode m_mode;
	int m_currentChannel = -1; // For edit mode
	QString m_currentCanvas; // For edit mode

	QLineEdit *m_nameEdit;
	QComboBox *m_typeCombo;
	QComboBox *m_canvasCombo;
	QComboBox *m_channelCombo;
	QPushButton *m_okButton;
	QPushButton *m_cancelButton;
	QLabel *m_errorLabel;
	QCheckBox *m_openPropertiesCheck;
	QCheckBox *m_mutedCheck;
};
