#pragma once

// ============================================================================
// SMixerFader — Vertical volume fader (QSlider subclass)
// ============================================================================

#include <QSlider>

namespace super {

class SMixerFader : public QSlider {
	Q_OBJECT

public:
	explicit SMixerFader(QWidget *parent = nullptr);

	// --- Volume Control ---
	void setVolume(float linear_volume);
	float volume() const;
	float volumeDb() const;
	void setNormalized(float norm);
	float normalized() const;

signals:
	void volumeChanged(float linear_volume);
	void faderMoved(int value); // 0-1000

protected:
	void paintEvent(QPaintEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	// We keep QSlider's mousePress/Move/Release for logic!

private:
	void updateVolumeFromSlider();

	bool m_updating = false;

	// dB marks
	static constexpr int DB_MARKS[] = {6, 3, 0, -3, -6, -9, -12, -24, -48, -60};
	static constexpr int DB_MARKS_COUNT = 10;
};

} // namespace super
