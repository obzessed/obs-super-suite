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
	int getChannel() const;
	bool shouldOpenProperties() const;
	void setOpenProperties(bool open);
	bool shouldStartMuted() const;
	void setStartMuted(bool muted);

	// Set channels that are already in use (will be greyed out)
	void setOccupiedChannels(const QSet<int> &channels);
	
	// For edit mode: the channel being edited (always enabled)
	void setCurrentChannel(int channel);

private slots:
	void validateInput();

private:
	void setupUi();
	void populateChannels();

	Mode m_mode;
	int m_currentChannel = -1; // For edit mode
	QSet<int> m_occupiedChannels;

	QLineEdit *m_nameEdit;
	QComboBox *m_channelCombo;
	QPushButton *m_okButton;
	QPushButton *m_cancelButton;
	QLabel *m_errorLabel;
	QCheckBox *m_openPropertiesCheck;
	QCheckBox *m_mutedCheck;
};
