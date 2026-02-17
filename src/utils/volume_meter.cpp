#include "volume_meter.hpp"

#include <QApplication>
#include <QPainter>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QPointer>
#include <QMutexLocker>
#include <QFontMetrics>
#include <QStyleOption>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <algorithm>
#include <cmath>

QPointer<QTimer> VolumeMeter::updateTimer = nullptr;

namespace {
constexpr int INDICATOR_THICKNESS = 3;
constexpr int CLIP_FLASH_DURATION_MS = 1000;
constexpr int TICK_SIZE = 2;
constexpr int TICK_DB_INTERVAL = 6;
} // namespace

static inline QColor color_from_int(long long val)
{
	QColor color(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
	color.setAlpha(255);
	return color;
}

VolumeMeter::VolumeMeter(QWidget *parent, obs_source_t *source, Style style)
	: QWidget(parent),
	  style(style),
	  obsVolumeMeter(obs_volmeter_create(OBS_FADER_LOG))
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setFocusPolicy(Qt::NoFocus);

	applyStyle();

	minimumLevel = -60.0;
	warningLevel = -20.0;
	errorLevel = -9.0;
	clipLevel = 0.0;
	minimumInputLevel = -50.0;
	peakDecayRate = 11.76;
	magnitudeIntegrationTime = 0.3;
	peakHoldDuration = 20.0;
	inputPeakHoldDuration = 1.0;
	meterThickness = 3;
	meterFontScaling = 0.8;
	channels = (int)audio_output_get_channels(obs_get_audio());

	if (source) {
		weakSource = obs_source_get_weak_source(source);
		obs_volmeter_add_callback(obsVolumeMeter, obsVolMeterChanged, this);
		obs_volmeter_attach_source(obsVolumeMeter, source);
	}

	resetLevels();

	if (!updateTimer) {
		updateTimer = new QTimer(qApp);
		updateTimer->setTimerType(Qt::PreciseTimer);
		updateTimer->start(16); // ~60 FPS for smooth updates
	}

	connect(updateTimer, &QTimer::timeout, this, [this]() {
		if (needLayoutChange()) {
			doLayout();
			update();
		} else {
			update(getBarRect());
		}
	});

	doLayout();
}

void VolumeMeter::applyStyle()
{
	switch (style) {
	case Style::Modern:
		backgroundNominalColor = QColor(0x2d, 0x5a, 0x2d); // Dark green
		backgroundWarningColor = QColor(0x5a, 0x5a, 0x2d); // Dark yellow
		backgroundErrorColor = QColor(0x5a, 0x2d, 0x2d);   // Dark red
		foregroundNominalColor = QColor(0x4c, 0xff, 0x4c); // Bright green
		foregroundWarningColor = QColor(0xff, 0xff, 0x4c); // Bright yellow
		foregroundErrorColor = QColor(0xff, 0x4c, 0x4c);   // Bright red
		clipColor = QColor(0xff, 0xff, 0xff);              // White
		magnitudeColor = QColor(0x00, 0x00, 0x00);         // Black
		majorTickColor = QColor(0x00, 0x00, 0x00);         // Black
		minorTickColor = QColor(0x66, 0x66, 0x66);         // Gray
		break;
	case Style::Vintage:
		backgroundNominalColor = QColor(0x4a, 0x3a, 0x1a); // Dark brown
		backgroundWarningColor = QColor(0x6a, 0x5a, 0x2a); // Brown-yellow
		backgroundErrorColor = QColor(0x5a, 0x2a, 0x2a);   // Brown-red
		foregroundNominalColor = QColor(0x8a, 0x7a, 0x4a); // Light brown
		foregroundWarningColor = QColor(0xba, 0xaa, 0x5a); // Yellow-brown
		foregroundErrorColor = QColor(0xaa, 0x5a, 0x5a);   // Red-brown
		clipColor = QColor(0xff, 0xff, 0xff);              // White
		magnitudeColor = QColor(0x00, 0x00, 0x00);         // Black
		majorTickColor = QColor(0x00, 0x00, 0x00);         // Black
		minorTickColor = QColor(0x66, 0x66, 0x66);         // Gray
		break;
	case Style::Analog:
		backgroundNominalColor = QColor(0x00, 0x40, 0x00); // Dark green
		backgroundWarningColor = QColor(0x40, 0x40, 0x00); // Olive
		backgroundErrorColor = QColor(0x40, 0x00, 0x00);   // Dark red
		foregroundNominalColor = QColor(0x00, 0xff, 0x00); // Bright green
		foregroundWarningColor = QColor(0xff, 0xff, 0x00); // Yellow
		foregroundErrorColor = QColor(0xff, 0x00, 0x00);   // Red
		clipColor = QColor(0xff, 0xff, 0xff);              // White
		magnitudeColor = QColor(0x00, 0x00, 0x00);         // Black
		majorTickColor = QColor(0xff, 0xff, 0xff);         // White ticks for contrast
		minorTickColor = QColor(0x80, 0x80, 0x80);         // Gray
		break;
	case Style::Fluid:
		backgroundNominalColor = QColor(0x00, 0x2a, 0x5a); // Dark blue
		backgroundWarningColor = QColor(0x2a, 0x2a, 0x5a); // Blue-purple
		backgroundErrorColor = QColor(0x5a, 0x00, 0x5a);   // Purple
		foregroundNominalColor = QColor(0x00, 0x7f, 0xff); // Light blue
		foregroundWarningColor = QColor(0x7f, 0x7f, 0xff); // Light purple
		foregroundErrorColor = QColor(0xff, 0x00, 0xff);   // Magenta
		clipColor = QColor(0xff, 0xff, 0xff);              // White
		magnitudeColor = QColor(0x00, 0x00, 0x00);         // Black
		majorTickColor = QColor(0x00, 0x00, 0x00);         // Black
		minorTickColor = QColor(0x66, 0x66, 0x66);         // Gray
		break;
	}
}

VolumeMeter::Style VolumeMeter::getStyle() const
{
	return style;
}

void VolumeMeter::setStyle(Style s)
{
	if (style == s)
		return;
	style = s;
	applyStyle();
	updateBackgroundCache(true);
	update();
}

VolumeMeter::~VolumeMeter()
{
	if (obsVolumeMeter) {
		obs_volmeter_remove_callback(obsVolumeMeter, obsVolMeterChanged, this);
		obs_volmeter_detach_source(obsVolumeMeter);
		obs_volmeter_destroy(obsVolumeMeter);
	}
	if (weakSource) {
		obs_weak_source_release(weakSource);
	}
}

void VolumeMeter::setLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
			    const float inputPeak[MAX_AUDIO_CHANNELS])
{
	uint64_t ts = os_gettime_ns();
	QMutexLocker locker(&dataMutex);

	currentLastUpdateTime = ts;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = magnitude[channelNr];
		currentPeak[channelNr] = peak[channelNr];
		currentInputPeak[channelNr] = inputPeak[channelNr];
	}

	locker.unlock();
	calculateBallistics(ts);
}

void VolumeMeter::obsVolMeterChanged(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				     const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	VolumeMeter *meter = static_cast<VolumeMeter *>(data);
	meter->setLevels(magnitude, peak, inputPeak);
}

void VolumeMeter::obsSourceDestroyed(void *data, calldata_t *)
{
	VolumeMeter *self = static_cast<VolumeMeter *>(data);
	QMetaObject::invokeMethod(self, "handleSourceDestroyed", Qt::QueuedConnection);
}

void VolumeMeter::handleSourceDestroyed()
{
	// Handle source destruction - perhaps emit signal or reset
	resetLevels();
	update();
}

void VolumeMeter::resetLevels()
{
	currentLastUpdateTime = 0;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = -INFINITY;
		currentPeak[channelNr] = -INFINITY;
		currentInputPeak[channelNr] = -INFINITY;
		displayMagnitude[channelNr] = -INFINITY;
		displayPeak[channelNr] = -INFINITY;
		displayPeakHold[channelNr] = -INFINITY;
		displayPeakHoldLastUpdateTime[channelNr] = 0;
		displayInputPeakHold[channelNr] = -INFINITY;
		displayInputPeakHoldLastUpdateTime[channelNr] = 0;
	}
}

bool VolumeMeter::needLayoutChange()
{
	int currentNrAudioChannels = obs_volmeter_get_nr_channels(obsVolumeMeter);
	if (!currentNrAudioChannels) {
		struct obs_audio_info oai;
		obs_get_audio_info(&oai);
		currentNrAudioChannels = (oai.speakers == SPEAKERS_MONO) ? 1 : 2;
	}
	if (displayNrAudioChannels != currentNrAudioChannels) {
		displayNrAudioChannels = currentNrAudioChannels;
		return true;
	}
	return false;
}

void VolumeMeter::setVertical(bool vertical_)
{
	if (vertical == vertical_)
		return;
	vertical = vertical_;
	doLayout();
}

void VolumeMeter::setMuted(bool mute)
{
	if (muted == mute)
		return;
	muted = mute;
	update();
}

void VolumeMeter::setPeakMeterType(enum obs_peak_meter_type peakMeterType)
{
	obs_volmeter_set_peak_meter_type(obsVolumeMeter, peakMeterType);
	switch (peakMeterType) {
	case TRUE_PEAK_METER:
		setErrorLevel(-2.0);
		setWarningLevel(-13.0);
		break;
	case SAMPLE_PEAK_METER:
	default:
		setErrorLevel(-9.0);
		setWarningLevel(-20.0);
		break;
	}
	bool forceUpdate = true;
	updateBackgroundCache(forceUpdate);
}

QRect VolumeMeter::getBarRect() const
{
	QRect barRect = rect();
	if (vertical) {
		barRect.setWidth(displayNrAudioChannels * (meterThickness + 1) - 1);
	} else {
		barRect.setHeight(displayNrAudioChannels * (meterThickness + 1) - 1);
	}
	return barRect;
}

void VolumeMeter::doLayout()
{
	QMutexLocker locker(&dataMutex);

	if (displayNrAudioChannels) {
		int meterSize = std::floor(22 / displayNrAudioChannels);
		meterThickness = std::clamp(meterSize, 3, 6);
	}

	tickFont = font();
	QFontInfo info(tickFont);
	tickFont.setPointSizeF(info.pointSizeF() * meterFontScaling);

	QFontMetrics metrics(tickFont);
	tickTextTokenRect = metrics.boundingRect(" -88 ");

	updateBackgroundCache();
	resetLevels();
	updateGeometry();
}

bool VolumeMeter::detectIdle(uint64_t ts)
{
	double secondsSinceLastUpdate = (ts - currentLastUpdateTime) * 1e-9;
	if (secondsSinceLastUpdate > 0.5) {
		resetLevels();
		return true;
	}
	return false;
}

void VolumeMeter::calculateBallisticsForChannel(int channelNr, uint64_t ts, qreal timeSinceLastRedraw)
{
	if (currentPeak[channelNr] >= displayPeak[channelNr] || std::isnan(displayPeak[channelNr])) {
		displayPeak[channelNr] = currentPeak[channelNr];
	} else {
		float decay = float(peakDecayRate * timeSinceLastRedraw);
		displayPeak[channelNr] =
			std::clamp(displayPeak[channelNr] - decay, std::min(currentPeak[channelNr], 0.f), 0.f);
	}

	if (currentPeak[channelNr] >= displayPeakHold[channelNr] || !std::isfinite(displayPeakHold[channelNr])) {
		displayPeakHold[channelNr] = currentPeak[channelNr];
		displayPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		qreal timeSinceLastPeak = (ts - displayPeakHoldLastUpdateTime[channelNr]) * 1e-9;
		if (timeSinceLastPeak > peakHoldDuration) {
			displayPeakHold[channelNr] = currentPeak[channelNr];
			displayPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] ||
	    !std::isfinite(displayInputPeakHold[channelNr])) {
		displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
		displayInputPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		qreal timeSinceLastPeak = (ts - displayInputPeakHoldLastUpdateTime[channelNr]) * 1e-9;
		if (timeSinceLastPeak > inputPeakHoldDuration) {
			displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
			displayInputPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (!std::isfinite(displayMagnitude[channelNr])) {
		displayMagnitude[channelNr] = currentMagnitude[channelNr];
	} else {
		float attack = float((currentMagnitude[channelNr] - displayMagnitude[channelNr]) *
				     (timeSinceLastRedraw / magnitudeIntegrationTime) * 0.99);
		displayMagnitude[channelNr] =
			std::clamp(displayMagnitude[channelNr] + attack, (float)minimumLevel, 0.f);
	}
}

void VolumeMeter::calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw)
{
	QMutexLocker locker(&dataMutex);
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		calculateBallisticsForChannel(channelNr, ts, timeSinceLastRedraw);
	}
}

QColor VolumeMeter::getPeakColor(float peakHold)
{
	if (peakHold < minimumInputLevel)
		return backgroundNominalColor;
	else if (peakHold < warningLevel)
		return foregroundNominalColor;
	else if (peakHold < errorLevel)
		return foregroundWarningColor;
	else if (peakHold < clipLevel)
		return foregroundErrorColor;
	else
		return clipColor;
}

void VolumeMeter::paintHTicks(QPainter &painter, int x, int y, int width)
{
	qreal scale = width / minimumLevel;
	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	for (int i = 0; i >= minimumLevel; i -= TICK_DB_INTERVAL) {
		int position = int(x + width - (i * scale) - 1);
		QString str = QString::number(i);
		QRect textBounds = metrics.boundingRect(str);
		int pos = (i == 0) ? position - textBounds.width() : std::max(0, position - (textBounds.width() / 2));
		painter.drawText(pos, y + 4 + metrics.capHeight(), str);
		painter.drawLine(position, y, position, y + TICK_SIZE);
	}
}

void VolumeMeter::paintVTicks(QPainter &painter, int x, int y, int height)
{
	qreal scale = height / minimumLevel;
	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	for (int i = 0; i >= minimumLevel; i -= TICK_DB_INTERVAL) {
		int position = y + int(i * scale);
		QString str = QString::number(i);
		if (i == 0)
			painter.drawText(x + 10, position + metrics.capHeight(), str);
		else
			painter.drawText(x + 8, position + (metrics.capHeight() / 2), str);
		painter.drawLine(x, position, x + TICK_SIZE, position);
	}
}

void VolumeMeter::updateBackgroundCache(bool force)
{
	if (!force && size().isEmpty())
		return;
	if (!force && backgroundCache.size() == size() && !backgroundCache.isNull())
		return;
	if (!force && displayNrAudioChannels <= 0)
		return;

	backgroundCache = QPixmap(size() * devicePixelRatioF());
	backgroundCache.setDevicePixelRatio(devicePixelRatioF());
	backgroundCache.fill(palette().color(QPalette::Window));

	QPainter bg(&backgroundCache);
	QRect widgetRect = rect();

	if (vertical) {
		paintVTicks(bg, displayNrAudioChannels * (meterThickness + 1) - 1, 0,
			    widgetRect.height() - (INDICATOR_THICKNESS + 3));
	} else {
		paintHTicks(bg, INDICATOR_THICKNESS + 3, displayNrAudioChannels * (meterThickness + 1) - 1,
			    widgetRect.width() - (INDICATOR_THICKNESS + 3));
	}

	int meterStart = INDICATOR_THICKNESS + 2;
	int meterLength = vertical ? rect().height() - (INDICATOR_THICKNESS + 2)
				   : rect().width() - (INDICATOR_THICKNESS + 2);
	qreal scale = meterLength / minimumLevel;
	int warningPosition = meterLength - convertToInt(warningLevel * scale);
	int errorPosition = meterLength - convertToInt(errorLevel * scale);

	for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {
		int channelOffset = channelNr * (meterThickness + 1);
		if (vertical) {
			bg.fillRect(channelOffset, meterLength, meterThickness, -meterLength, backgroundErrorColor);
			bg.fillRect(channelOffset, meterLength, meterThickness,
				    -(warningPosition + (errorPosition - warningPosition)), backgroundWarningColor);
			bg.fillRect(channelOffset, meterLength, meterThickness, -warningPosition,
				    backgroundNominalColor);
		} else {
			int nominalLength = warningPosition;
			int warningLength = nominalLength + (errorPosition - warningPosition);
			bg.fillRect(meterStart, channelOffset, meterLength, meterThickness, backgroundErrorColor);
			bg.fillRect(meterStart, channelOffset, warningLength, meterThickness, backgroundWarningColor);
			bg.fillRect(meterStart, channelOffset, nominalLength, meterThickness, backgroundNominalColor);
		}
	}
}

int VolumeMeter::convertToInt(float number) const
{
	constexpr int min = std::numeric_limits<int>::min();
	constexpr int max = std::numeric_limits<int>::max();
	if (number >= (float)max)
		return max;
	else if (number <= min)
		return min;
	else
		return int(number);
}

void VolumeMeter::paintEvent(QPaintEvent *)
{
	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 1e-9;
	calculateBallistics(ts, timeSinceLastRedraw);
	bool idle = detectIdle(ts);

	QPainter painter(this);

	int meterStart = INDICATOR_THICKNESS + 2;
	int meterLength = vertical ? rect().height() - (INDICATOR_THICKNESS + 2)
				   : rect().width() - (INDICATOR_THICKNESS + 2);
	qreal scale = meterLength / minimumLevel;
	int warningPosition = meterLength - convertToInt(warningLevel * scale);
	int errorPosition = meterLength - convertToInt(errorLevel * scale);
	int clipPosition = meterLength - convertToInt(clipLevel * scale);

	painter.drawPixmap(0, 0, backgroundCache);

	for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {
		int channelNrFixed = (displayNrAudioChannels == 1 && channels > 2) ? 2 : channelNr;

		QMutexLocker locker(&dataMutex);
		float peak = displayPeak[channelNrFixed];
		float peakHold = displayPeakHold[channelNrFixed];
		float magnitude = displayMagnitude[channelNrFixed];
		locker.unlock();

		int peakPosition = meterLength - convertToInt(peak * scale);
		int peakHoldPosition = meterLength - convertToInt(peakHold * scale);
		int magnitudePosition = meterLength - convertToInt(magnitude * scale);

		if (clipping)
			peakPosition = meterLength;

		auto fill = [&](int pos, int length, const QColor &color) {
			if (vertical) {
				painter.fillRect(pos, meterLength, meterThickness, -length, color);
			} else {
				painter.fillRect(meterStart, pos, length, meterThickness, color);
			}
		};

		int channelOffset = channelNr * (meterThickness + 1);

		if (peakPosition >= clipPosition) {
			if (!clipping) {
				QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, [&]() { clipping = false; });
				clipping = true;
			}
			fill(channelOffset, meterLength, foregroundErrorColor);
		} else {
			if (peakPosition > errorPosition)
				fill(channelOffset, std::min(peakPosition, meterLength), foregroundErrorColor);
			if (peakPosition > warningPosition)
				fill(channelOffset,
				     std::min(peakPosition, warningPosition + (errorPosition - warningPosition)),
				     foregroundWarningColor);
			if (peakPosition > meterStart)
				fill(channelOffset, std::min(peakPosition, warningPosition), foregroundNominalColor);
		}

		// Peak hold
		QColor peakHoldColor = (peakHoldPosition >= errorPosition)     ? foregroundErrorColor
				       : (peakHoldPosition >= warningPosition) ? foregroundWarningColor
									       : foregroundNominalColor;
		if (peakHoldPosition - 3 > 0) {
			if (vertical)
				painter.fillRect(channelOffset, meterLength - peakHoldPosition - 3, meterThickness, 3,
						 peakHoldColor);
			else
				painter.fillRect(meterStart + peakHoldPosition - 3, channelOffset, 3, meterThickness,
						 peakHoldColor);
		}

		// Magnitude
		if (magnitudePosition - 3 >= 0) {
			if (vertical)
				painter.fillRect(channelOffset, meterLength - magnitudePosition - 3, meterThickness, 3,
						 magnitudeColor);
			else
				painter.fillRect(meterStart + magnitudePosition - 3, channelOffset, 3, meterThickness,
						 magnitudeColor);
		}

		if (!idle) {
			if (vertical)
				painter.fillRect(channelOffset, rect().height(), meterThickness, -INDICATOR_THICKNESS,
						 getPeakColor(displayInputPeakHold[channelNrFixed]));
			else
				painter.fillRect(0, channelOffset, INDICATOR_THICKNESS, meterThickness,
						 getPeakColor(displayInputPeakHold[channelNrFixed]));
		}
	}

	lastRedrawTime = ts;
}

void VolumeMeter::resizeEvent(QResizeEvent *event)
{
	updateBackgroundCache();
	QWidget::resizeEvent(event);
}

void VolumeMeter::mousePressEvent(QMouseEvent *event)
{
	setFocus(Qt::MouseFocusReason);
	event->accept();
}

QSize VolumeMeter::sizeHint() const
{
	QRect meterRect = getBarRect();
	int labelTotal = std::abs(int(minimumLevel / TICK_DB_INTERVAL)) + 1;

	if (vertical) {
		int width = meterRect.width() + tickTextTokenRect.width() + TICK_SIZE + 10;
		int height = (labelTotal * tickTextTokenRect.height()) + INDICATOR_THICKNESS;
		return QSize(width, int(height * 1.1));
	} else {
		int width = (labelTotal * tickTextTokenRect.width()) + INDICATOR_THICKNESS;
		int height = meterRect.height() + tickTextTokenRect.height();
		return QSize(int(width * 1.1), height);
	}
}

QSize VolumeMeter::minimumSizeHint() const
{
	return sizeHint();
}

// Getter/Setter implementations
QColor VolumeMeter::getBackgroundNominalColor() const
{
	return backgroundNominalColor;
}
void VolumeMeter::setBackgroundNominalColor(QColor c)
{
	backgroundNominalColor = c;
	updateBackgroundCache(true);
}
QColor VolumeMeter::getBackgroundWarningColor() const
{
	return backgroundWarningColor;
}
void VolumeMeter::setBackgroundWarningColor(QColor c)
{
	backgroundWarningColor = c;
	updateBackgroundCache(true);
}
QColor VolumeMeter::getBackgroundErrorColor() const
{
	return backgroundErrorColor;
}
void VolumeMeter::setBackgroundErrorColor(QColor c)
{
	backgroundErrorColor = c;
	updateBackgroundCache(true);
}
QColor VolumeMeter::getForegroundNominalColor() const
{
	return foregroundNominalColor;
}
void VolumeMeter::setForegroundNominalColor(QColor c)
{
	foregroundNominalColor = c;
}
QColor VolumeMeter::getForegroundWarningColor() const
{
	return foregroundWarningColor;
}
void VolumeMeter::setForegroundWarningColor(QColor c)
{
	foregroundWarningColor = c;
}
QColor VolumeMeter::getForegroundErrorColor() const
{
	return foregroundErrorColor;
}
void VolumeMeter::setForegroundErrorColor(QColor c)
{
	foregroundErrorColor = c;
}
QColor VolumeMeter::getClipColor() const
{
	return clipColor;
}
void VolumeMeter::setClipColor(QColor c)
{
	clipColor = c;
}
QColor VolumeMeter::getMagnitudeColor() const
{
	return magnitudeColor;
}
void VolumeMeter::setMagnitudeColor(QColor c)
{
	magnitudeColor = c;
}
QColor VolumeMeter::getMajorTickColor() const
{
	return majorTickColor;
}
void VolumeMeter::setMajorTickColor(QColor c)
{
	majorTickColor = c;
	updateBackgroundCache(true);
}
QColor VolumeMeter::getMinorTickColor() const
{
	return minorTickColor;
}
void VolumeMeter::setMinorTickColor(QColor c)
{
	minorTickColor = c;
}

qreal VolumeMeter::getWarningLevel() const
{
	return warningLevel;
}
void VolumeMeter::setWarningLevel(qreal v)
{
	warningLevel = v;
	updateBackgroundCache(true);
}
qreal VolumeMeter::getErrorLevel() const
{
	return errorLevel;
}
void VolumeMeter::setErrorLevel(qreal v)
{
	errorLevel = v;
	updateBackgroundCache(true);
}
qreal VolumeMeter::getMinimumLevel() const
{
	return minimumLevel;
}
void VolumeMeter::setMinimumLevel(qreal v)
{
	minimumLevel = v;
	updateBackgroundCache(true);
}
qreal VolumeMeter::getClipLevel() const
{
	return clipLevel;
}
void VolumeMeter::setClipLevel(qreal v)
{
	clipLevel = v;
}
qreal VolumeMeter::getMinimumInputLevel() const
{
	return minimumInputLevel;
}
void VolumeMeter::setMinimumInputLevel(qreal v)
{
	minimumInputLevel = v;
}
qreal VolumeMeter::getPeakDecayRate() const
{
	return peakDecayRate;
}
void VolumeMeter::setPeakDecayRate(qreal v)
{
	peakDecayRate = v;
}
qreal VolumeMeter::getMagnitudeIntegrationTime() const
{
	return magnitudeIntegrationTime;
}
void VolumeMeter::setMagnitudeIntegrationTime(qreal v)
{
	magnitudeIntegrationTime = v;
}
qreal VolumeMeter::getPeakHoldDuration() const
{
	return peakHoldDuration;
}
void VolumeMeter::setPeakHoldDuration(qreal v)
{
	peakHoldDuration = v;
}
qreal VolumeMeter::getInputPeakHoldDuration() const
{
	return inputPeakHoldDuration;
}
void VolumeMeter::setInputPeakHoldDuration(qreal v)
{
	inputPeakHoldDuration = v;
}

int VolumeMeter::getMeterThickness() const
{
	return meterThickness;
}
void VolumeMeter::setMeterThickness(int v)
{
	meterThickness = v;
	doLayout();
}
qreal VolumeMeter::getMeterFontScaling() const
{
	return meterFontScaling;
}
void VolumeMeter::setMeterFontScaling(qreal v)
{
	meterFontScaling = v;
	doLayout();
}
bool VolumeMeter::getVertical() const
{
	return vertical;
}