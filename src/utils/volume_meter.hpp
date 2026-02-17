#pragma once

#include <QWidget>
#include <QMutex>
#include <QColor>
#include <obs.hpp>

class VolumeMeter : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor backgroundNominalColor READ getBackgroundNominalColor WRITE setBackgroundNominalColor)
	Q_PROPERTY(QColor backgroundWarningColor READ getBackgroundWarningColor WRITE setBackgroundWarningColor)
	Q_PROPERTY(QColor backgroundErrorColor READ getBackgroundErrorColor WRITE setBackgroundErrorColor)
	Q_PROPERTY(QColor foregroundNominalColor READ getForegroundNominalColor WRITE setForegroundNominalColor)
	Q_PROPERTY(QColor foregroundWarningColor READ getForegroundWarningColor WRITE setForegroundWarningColor)
	Q_PROPERTY(QColor foregroundErrorColor READ getForegroundErrorColor WRITE setForegroundErrorColor)
	Q_PROPERTY(QColor clipColor READ getClipColor WRITE setClipColor)
	Q_PROPERTY(QColor magnitudeColor READ getMagnitudeColor WRITE setMagnitudeColor)
	Q_PROPERTY(QColor majorTickColor READ getMajorTickColor WRITE setMajorTickColor)
	Q_PROPERTY(QColor minorTickColor READ getMinorTickColor WRITE setMinorTickColor)
	Q_PROPERTY(qreal warningLevel READ getWarningLevel WRITE setWarningLevel)
	Q_PROPERTY(qreal errorLevel READ getErrorLevel WRITE setErrorLevel)
	Q_PROPERTY(qreal minimumLevel READ getMinimumLevel WRITE setMinimumLevel)
	Q_PROPERTY(qreal clipLevel READ getClipLevel WRITE setClipLevel)
	Q_PROPERTY(qreal minimumInputLevel READ getMinimumInputLevel WRITE setMinimumInputLevel)
	Q_PROPERTY(qreal peakDecayRate READ getPeakDecayRate WRITE setPeakDecayRate)
	Q_PROPERTY(qreal magnitudeIntegrationTime READ getMagnitudeIntegrationTime WRITE setMagnitudeIntegrationTime)
	Q_PROPERTY(qreal peakHoldDuration READ getPeakHoldDuration WRITE setPeakHoldDuration)
	Q_PROPERTY(qreal inputPeakHoldDuration READ getInputPeakHoldDuration WRITE setInputPeakHoldDuration)
	Q_PROPERTY(int meterThickness READ getMeterThickness WRITE setMeterThickness)
	Q_PROPERTY(qreal meterFontScaling READ getMeterFontScaling WRITE setMeterFontScaling)
	Q_PROPERTY(bool vertical READ getVertical WRITE setVertical)
	Q_PROPERTY(Style style READ getStyle WRITE setStyle)

public:
	enum class Style { Modern, Vintage, Analog, Fluid };
	Q_ENUM(Style)

	explicit VolumeMeter(QWidget *parent = nullptr, obs_source_t *source = nullptr, Style style = Style::Modern);
	~VolumeMeter() override;

	// Colors
	QColor getBackgroundNominalColor() const;
	void setBackgroundNominalColor(QColor c);
	QColor getBackgroundWarningColor() const;
	void setBackgroundWarningColor(QColor c);
	QColor getBackgroundErrorColor() const;
	void setBackgroundErrorColor(QColor c);
	QColor getForegroundNominalColor() const;
	void setForegroundNominalColor(QColor c);
	QColor getForegroundWarningColor() const;
	void setForegroundWarningColor(QColor c);
	QColor getForegroundErrorColor() const;
	void setForegroundErrorColor(QColor c);
	QColor getClipColor() const;
	void setClipColor(QColor c);
	QColor getMagnitudeColor() const;
	void setMagnitudeColor(QColor c);
	QColor getMajorTickColor() const;
	void setMajorTickColor(QColor c);
	QColor getMinorTickColor() const;
	void setMinorTickColor(QColor c);

	// Levels
	qreal getWarningLevel() const;
	void setWarningLevel(qreal v);
	qreal getErrorLevel() const;
	void setErrorLevel(qreal v);
	qreal getMinimumLevel() const;
	void setMinimumLevel(qreal v);
	qreal getClipLevel() const;
	void setClipLevel(qreal v);
	qreal getMinimumInputLevel() const;
	void setMinimumInputLevel(qreal v);
	qreal getPeakDecayRate() const;
	void setPeakDecayRate(qreal v);
	qreal getMagnitudeIntegrationTime() const;
	void setMagnitudeIntegrationTime(qreal v);
	qreal getPeakHoldDuration() const;
	void setPeakHoldDuration(qreal v);
	qreal getInputPeakHoldDuration() const;
	void setInputPeakHoldDuration(qreal v);

	// Appearance
	int getMeterThickness() const;
	void setMeterThickness(int v);
	qreal getMeterFontScaling() const;
	void setMeterFontScaling(qreal v);
	bool getVertical() const;
	void setVertical(bool v);
	Style getStyle() const;
	void setStyle(Style s);

	// State
	void setMuted(bool mute);
	void setPeakMeterType(enum obs_peak_meter_type peakMeterType);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
	static void obsVolMeterChanged(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				       const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS]);
	static void obsSourceDestroyed(void *data, calldata_t *cd);
	void handleSourceDestroyed();

	void setLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
		       const float inputPeak[MAX_AUDIO_CHANNELS]);
	void resetLevels();
	bool needLayoutChange();
	void doLayout();
	bool detectIdle(uint64_t ts);
	void calculateBallisticsForChannel(int channelNr, uint64_t ts, qreal timeSinceLastRedraw);
	void calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw = 0.016);
	QColor getPeakColor(float peakHold);
	void paintHTicks(QPainter &painter, int x, int y, int width);
	void paintVTicks(QPainter &painter, int x, int y, int height);
	void updateBackgroundCache(bool force = false);
	int convertToInt(float number) const;
	QRect getBarRect() const;
	void applyStyle();

	obs_weak_source_t *weakSource = nullptr;
	obs_volmeter_t *obsVolumeMeter = nullptr;

	// Colors
	QColor backgroundNominalColor, backgroundWarningColor, backgroundErrorColor;
	QColor foregroundNominalColor, foregroundWarningColor, foregroundErrorColor;
	QColor clipColor, magnitudeColor, majorTickColor, minorTickColor;

	// Levels
	qreal minimumLevel, warningLevel, errorLevel, clipLevel, minimumInputLevel;

	// Dynamics
	qreal peakDecayRate, magnitudeIntegrationTime, peakHoldDuration, inputPeakHoldDuration;

	// Appearance
	int meterThickness;
	qreal meterFontScaling;
	bool vertical = false;
	Style style = Style::Modern;
	QFont tickFont;
	QRect tickTextTokenRect;

	// State
	bool muted = false;
	bool clipping = false;
	int channels = 2;
	int displayNrAudioChannels = 0;

	// Data
	float currentMagnitude[MAX_AUDIO_CHANNELS];
	float currentPeak[MAX_AUDIO_CHANNELS];
	float currentInputPeak[MAX_AUDIO_CHANNELS];
	float displayMagnitude[MAX_AUDIO_CHANNELS];
	float displayPeak[MAX_AUDIO_CHANNELS];
	float displayPeakHold[MAX_AUDIO_CHANNELS];
	float displayInputPeakHold[MAX_AUDIO_CHANNELS];
	uint64_t displayPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
	uint64_t displayInputPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
	uint64_t currentLastUpdateTime = 0;
	uint64_t lastRedrawTime = 0;

	QPixmap backgroundCache;
	QMutex dataMutex;

	static QPointer<QTimer> updateTimer;
};