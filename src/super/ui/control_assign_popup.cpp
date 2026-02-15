#include "control_assign_popup.hpp"
#include "../core/control_registry.hpp"
#include "../core/control_port.hpp"
#include "../../utils/midi/midi_backend.hpp"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QContextMenuEvent>

namespace super {

// ===== ActivityDot ========================================================
ActivityDot::ActivityDot(const QColor &color, QWidget *parent)
	: QWidget(parent), m_color(color)
{
	setFixedSize(8, 8);
	m_fade_timer.setInterval(30);
	connect(&m_fade_timer, &QTimer::timeout, this, [this]{
		m_opacity = qMax(0.15, m_opacity - 0.08);
		update();
		if (m_opacity <= 0.16) m_fade_timer.stop();
	});
}
void ActivityDot::pulse() { m_opacity = 1.0; update(); m_fade_timer.start(); }
void ActivityDot::paintEvent(QPaintEvent *) {
	QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(QColor(m_color.red(),m_color.green(),m_color.blue(),int(m_opacity*255)));
	p.setPen(Qt::NoPen); p.drawEllipse(rect());
}

// ===== PipelineVisualDialog ===============================================
PipelineVisualDialog::PipelineVisualDialog(const QString &name,
	double out_min, double out_max, QWidget *parent)
	: QDialog(parent, Qt::Dialog | Qt::WindowCloseButtonHint)
	, m_name(name), m_out_min(out_min), m_out_max(out_max)
{
	setWindowTitle(QString("Pipeline — %1").arg(name));
	setAttribute(Qt::WA_DeleteOnClose);
	resize(700, 280);
	setMinimumSize(400, 200);
	setStyleSheet("QDialog{background:rgb(22,22,30);}");
}

void PipelineVisualDialog::feed(int raw, const PipelinePreview &p) {
	// Rebuild columns if stage count changed
	int n_pre = p.after_pre_filter.size();
	int n_int = p.after_interp.size();
	int n_post = p.after_post_filter.size();
	// total = pre + Norm + interp + Map + post
	int total = n_pre + 1 + n_int + 1 + n_post;
	if (total != m_prev_col_count) {
		m_columns.clear();
		auto mk = [&](const QString &l, const QColor &col, double mn, double mx) {
			Column c; c.label = l; c.color = col;
			c.val_min = mn; c.val_max = mx;
			c.buf_in.resize(COL_BUF, 0); c.buf_out.resize(COL_BUF, 0);
			m_columns.append(c);
		};
		for (int i = 0; i < n_pre; i++)
			mk(QString("Pre #%1").arg(i+1), QColor(46,204,113), 0, 127);
		mk(QString("Norm\n%1-%2\u21920-1").arg(p.input_min).arg(p.input_max),
			QColor(180,140,255), 0, 127);
		for (int i = 0; i < n_int; i++)
			mk(QString("Interp #%1").arg(i+1), QColor(52,152,219), 0, 1.0);
		mk(QString("Map\n0-1\u2192%1-%2")
			.arg(p.output_min,0,'f',1).arg(p.output_max,0,'f',1),
			QColor(255,180,80), m_out_min, m_out_max);
		for (int i = 0; i < n_post; i++)
			mk(QString("Post #%1").arg(i+1), QColor(230,126,34), m_out_min, m_out_max);
		m_prev_col_count = total;
	}
	// Push values into columns
	int ci = 0;
	// Pre-filters
	for (int i = 0; i < n_pre; i++, ci++) {
		auto &c = m_columns[ci];
		c.last_in = (i==0) ? double(raw) : p.after_pre_filter[i-1];
		c.last_out = p.after_pre_filter[i];
		c.dimmed = (i < p.pre_filter_enabled.size()) && !p.pre_filter_enabled[i];
		c.buf_in[c.head] = c.last_in; c.buf_out[c.head] = c.last_out;
		c.head = (c.head + 1) % COL_BUF;
		if (c.head == 0) c.full = true;
	}
	// Norm column — shows raw→normalized compression
	{
		auto &c = m_columns[ci++];
		c.last_in = p.pre_filtered;        // raw domain (0-127)
		c.last_out = p.normalized * 127.0; // normalized back-scaled to raw domain
		c.buf_in[c.head] = c.last_in; c.buf_out[c.head] = c.last_out;
		c.head = (c.head + 1) % COL_BUF;
		if (c.head == 0) c.full = true;
	}
	// Interps
	for (int i = 0; i < n_int; i++, ci++) {
		auto &c = m_columns[ci];
		c.last_in = (i==0) ? p.normalized : p.after_interp[i-1];
		c.last_out = p.after_interp[i];
		c.dimmed = (i < p.interp_enabled.size()) && !p.interp_enabled[i];
		c.buf_in[c.head] = c.last_in; c.buf_out[c.head] = c.last_out;
		c.head = (c.head + 1) % COL_BUF;
		if (c.head == 0) c.full = true;
	}
	// Map column — shows interp→output scaling (mirrors Norm pattern)
	{
		auto &c = m_columns[ci++];
		double interp_last = n_int > 0 ? p.after_interp.last() : p.normalized;
		double range = m_out_max - m_out_min;
		c.last_in = m_out_min + interp_last * range; // input projected to output domain
		c.last_out = p.mapped;                        // actual mapped output
		c.buf_in[c.head] = c.last_in; c.buf_out[c.head] = c.last_out;
		c.head = (c.head + 1) % COL_BUF;
		if (c.head == 0) c.full = true;
	}
	// Post-filters
	for (int i = 0; i < n_post; i++, ci++) {
		auto &c = m_columns[ci];
		c.last_in = (i==0) ? p.mapped : p.after_post_filter[i-1];
		c.last_out = p.after_post_filter[i];
		c.dimmed = (i < p.post_filter_enabled.size()) && !p.post_filter_enabled[i];
		c.buf_in[c.head] = c.last_in; c.buf_out[c.head] = c.last_out;
		c.head = (c.head + 1) % COL_BUF;
		if (c.head == 0) c.full = true;
	}
	m_raw = raw; m_final = p.final_value;
	update();
}

void PipelineVisualDialog::draw_col_graph(QPainter &p, const QRect &area, const Column &c) {
	int count = c.full ? COL_BUF : c.head;
	if (count < 2) return;
	double range = (c.val_max == c.val_min) ? 1.0 : (c.val_max - c.val_min);
	auto build_pts = [&](const QVector<double> &buf) -> QVector<QPointF> {
		QVector<QPointF> pts;
		for (int i = 0; i < count; i++) {
			int idx = c.full ? (c.head + i) % COL_BUF : i;
			double norm = qBound(0.0, (buf[idx] - c.val_min) / range, 1.0);
			double x = area.left() + double(i) / (count - 1) * area.width();
			double y = area.top() + (1.0 - norm) * area.height();
			pts.append(QPointF(x, y));
		}
		return pts;
	};
	// IN series (dim)
	auto pts_in = build_pts(c.buf_in);
	QColor dim = c.color; dim.setAlpha(80);
	p.setPen(QPen(dim, 1.0)); p.setBrush(Qt::NoBrush);
	p.drawPolyline(pts_in.data(), pts_in.size());
	// OUT series (bright + fill)
	auto pts_out = build_pts(c.buf_out);
	QColor fc = c.color; fc.setAlpha(20);
	QVector<QPointF> fp = pts_out;
	fp.append(QPointF(pts_out.last().x(), area.bottom()));
	fp.append(QPointF(pts_out.first().x(), area.bottom()));
	p.setPen(Qt::NoPen); p.setBrush(fc);
	p.drawPolygon(fp.data(), fp.size());
	p.setPen(QPen(c.color, 1.5)); p.setBrush(Qt::NoBrush);
	p.drawPolyline(pts_out.data(), pts_out.size());
}

void PipelineVisualDialog::draw_fader(QPainter &p, const QRect &area,
	double val, double vmin, double vmax, const QColor &col, const QString &lbl)
{
	p.setPen(Qt::NoPen); p.setBrush(QColor(30,30,40));
	p.drawRoundedRect(area, 3, 3);
	double range = (vmax == vmin) ? 1.0 : (vmax - vmin);
	double norm = qBound(0.0, (val - vmin) / range, 1.0);
	int fill_h = int(norm * (area.height() - 4));
	QLinearGradient grad(area.left(), area.bottom(), area.left(), area.top());
	QColor dim = col; dim.setAlpha(80);
	grad.setColorAt(0, dim); grad.setColorAt(1, col);
	p.setBrush(grad);
	p.drawRoundedRect(area.left()+2, area.bottom()-2-fill_h, area.width()-4, fill_h, 2, 2);
	p.setPen(QColor(180,180,200));
	p.setFont(QFont("sans-serif", 7, QFont::Bold));
	p.drawText(area, Qt::AlignTop | Qt::AlignHCenter, lbl);
	p.setFont(QFont("monospace", 7));
	p.drawText(QRect(area.left(), area.bottom()-14, area.width(), 14),
		Qt::AlignCenter, QString::number(val, 'f', 2));
}

void PipelineVisualDialog::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	int w = width(), h = height();
	int mg = 8, gap = 6;
	int fader_w = 38;
	int label_h = 24;
	int value_h = 14;
	int n_cols = m_columns.size();

	// Layout: [IN fader] [→] [col0] [→] [col1] ... [→] [OUT fader]
	int content_w = w - 2*mg;
	int faders_total = 2 * (fader_w + gap); // IN and OUT faders
	int arrows_total = (n_cols + 1) * 12; // arrow space between items
	int graphs_total = content_w - faders_total - arrows_total;
	int col_w = (n_cols > 0) ? qMax(40, graphs_total / n_cols) : 0;

	int graph_top = mg + label_h + 2;
	int graph_bot = h - mg - value_h - 2;
	int graph_h = graph_bot - graph_top;

	int x = mg;

	// IN fader
	QRect in_fader(x, graph_top, fader_w, graph_h);
	draw_fader(p, in_fader, m_raw, 0, 127, QColor(80,140,220), "IN");
	// Label
	p.setPen(QColor(130,170,255));
	p.setFont(QFont("sans-serif", 8, QFont::Bold));
	p.drawText(QRect(x, mg, fader_w, label_h), Qt::AlignCenter, "MIDI");
	x += fader_w + gap;

	// Draw columns
	for (int i = 0; i < n_cols; i++) {
		auto &c = m_columns[i];
		// Arrow
		p.setPen(QPen(QColor(100,100,120), 1.5));
		int ax = x + 3, ay = graph_top + graph_h / 2;
		p.drawLine(ax, ay, ax + 6, ay);
		p.drawLine(ax + 4, ay - 3, ax + 6, ay);
		p.drawLine(ax + 4, ay + 3, ax + 6, ay);
		x += 12;

		// Column background
		QRect col_area(x, graph_top, col_w, graph_h);
		p.setPen(Qt::NoPen); p.setBrush(QColor(18,18,26));
		p.drawRoundedRect(col_area.adjusted(-1,-1,1,1), 3, 3);

		if (c.dimmed) {
			// Greyed out — dark overlay + dashed center line
			p.setPen(Qt::NoPen); p.setBrush(QColor(30,30,38,180));
			p.drawRoundedRect(col_area, 3, 3);
			p.setPen(QPen(QColor(70,70,80), 0.5, Qt::DashLine));
			p.drawLine(col_area.left(), col_area.top()+graph_h/2,
				col_area.right(), col_area.top()+graph_h/2);
			// Label (dimmed)
			p.setPen(QColor(80,80,90));
			p.setFont(QFont("sans-serif", 6));
			p.drawText(QRect(x, mg, col_w, label_h), Qt::AlignCenter, c.label);
			// Value
			p.drawText(QRect(x, graph_bot+2, col_w, value_h), Qt::AlignCenter, "off");
		} else {
			// Grid
			p.setPen(QPen(QColor(40,40,50), 0.5, Qt::DotLine));
			p.drawLine(col_area.left(), col_area.top() + graph_h/2,
				col_area.right(), col_area.top() + graph_h/2);
			// Graph
			draw_col_graph(p, col_area, c);
			// Label
			p.setPen(c.color);
			p.setFont(QFont("sans-serif", 6, QFont::Bold));
			p.drawText(QRect(x, mg, col_w, label_h), Qt::AlignCenter, c.label);
			// Value
			p.setPen(QColor(160,160,180));
			p.setFont(QFont("monospace", 7));
			QString vtxt = QString("%1→%2").arg(c.last_in,0,'f',2).arg(c.last_out,0,'f',2);
			p.drawText(QRect(x, graph_bot + 2, col_w, value_h), Qt::AlignCenter, vtxt);
		}

		x += col_w;
	}

	// Arrow before OUT
	p.setPen(QPen(QColor(100,100,120), 1.5));
	int ax = x + 3 + gap, ay = graph_top + graph_h / 2;
	p.drawLine(ax, ay, ax + 6, ay);
	p.drawLine(ax + 4, ay - 3, ax + 6, ay);
	p.drawLine(ax + 4, ay + 3, ax + 6, ay);
	x += 12 + gap;

	// OUT fader
	int out_x = w - mg - fader_w;
	QRect out_fader(out_x, graph_top, fader_w, graph_h);
	draw_fader(p, out_fader, m_final, m_out_min, m_out_max, QColor(100,220,180), "OUT");
	p.setPen(QColor(100,220,180));
	p.setFont(QFont("sans-serif", 8, QFont::Bold));
	p.drawText(QRect(out_x, mg, fader_w, label_h), Qt::AlignCenter, "CTRL");
}

// ===== GraphDetailDialog ==================================================
static const char *GRAPH_DETAIL_STYLE =
	"QDialog{background:rgb(22,22,30);}";

GraphDetailDialog::GraphDetailDialog(const QString &title, const QColor &primary,
	const QColor &secondary, double val_min, double val_max, QWidget *parent)
	: QDialog(parent, Qt::Dialog | Qt::WindowCloseButtonHint)
	, m_title(title), m_primary_color(primary), m_secondary_color(secondary)
	, m_min(val_min), m_max(val_max)
{
	setWindowTitle(QString("Signal — %1").arg(title));
	setAttribute(Qt::WA_DeleteOnClose);
	resize(480, 260);
	setMinimumSize(300, 180);
	setStyleSheet(GRAPH_DETAIL_STYLE);
	m_primary.resize(BUF_SIZE, 0.0);
	m_secondary.resize(BUF_SIZE, 0.0);
}
void GraphDetailDialog::push(double primary, double secondary) {
	m_primary[m_head] = primary;
	m_secondary[m_head] = secondary;
	m_last_primary = primary;
	m_last_secondary = secondary;
	m_head = (m_head + 1) % BUF_SIZE;
	if (m_head == 0) m_full = true;
	update();
}
void GraphDetailDialog::draw_series(QPainter &p, const QVector<double> &buf,
	int head, bool full, const QColor &col, const QRect &area)
{
	int count = full ? BUF_SIZE : head;
	if (count < 2) return;
	double range = (m_max == m_min) ? 1.0 : (m_max - m_min);
	int x0 = area.left(), w = area.width(), h = area.height(), y0 = area.top();
	QVector<QPointF> pts;
	for (int i = 0; i < count; i++) {
		int idx = full ? (head + i) % BUF_SIZE : i;
		double norm = qBound(0.0, (buf[idx] - m_min) / range, 1.0);
		double x = x0 + double(i) / (count - 1) * w;
		double y = y0 + (1.0 - norm) * h;
		pts.append(QPointF(x, y));
	}
	QColor fc = col; fc.setAlpha(25);
	QVector<QPointF> fp = pts;
	fp.append(QPointF(pts.last().x(), y0 + h));
	fp.append(QPointF(pts.first().x(), y0 + h));
	p.setPen(Qt::NoPen); p.setBrush(fc);
	p.drawPolygon(fp.data(), fp.size());
	p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush);
	p.drawPolyline(pts.data(), pts.size());
}
void GraphDetailDialog::draw_fader_v(QPainter &p, const QRect &area, double val,
	const QColor &col, const QString &label)
{
	int x = area.x(), y = area.y(), w = area.width(), h = area.height();
	p.setPen(Qt::NoPen); p.setBrush(QColor(35,35,45));
	p.drawRoundedRect(area, 3, 3);
	double range = (m_max == m_min) ? 1.0 : (m_max - m_min);
	double norm = qBound(0.0, (val - m_min) / range, 1.0);
	int fill_h = int(norm * (h - 4));
	QLinearGradient grad(x, y + h, x, y);
	QColor dim = col; dim.setAlpha(80);
	grad.setColorAt(0, dim); grad.setColorAt(1, col);
	p.setBrush(grad);
	p.drawRoundedRect(x + 2, y + h - 2 - fill_h, w - 4, fill_h, 2, 2);
	p.setPen(QColor(180,180,200));
	p.setFont(QFont("sans-serif", 7, QFont::Bold));
	p.drawText(area, Qt::AlignTop | Qt::AlignHCenter, label);
	p.setFont(QFont("monospace", 7));
	p.drawText(QRect(x, y + h - 16, w, 16), Qt::AlignCenter,
		QString::number(val, 'f', 2));
}
void GraphDetailDialog::draw_fader_h(QPainter &p, const QRect &area, double val,
	const QColor &col, const QString &label)
{
	int x = area.x(), y = area.y(), w = area.width(), h = area.height();
	p.setPen(Qt::NoPen); p.setBrush(QColor(35,35,45));
	p.drawRoundedRect(area, 3, 3);
	double range = (m_max == m_min) ? 1.0 : (m_max - m_min);
	double norm = qBound(0.0, (val - m_min) / range, 1.0);
	int fill_w = int(norm * (w - 4));
	QLinearGradient grad(x, y, x + w, y);
	QColor dim = col; dim.setAlpha(80);
	grad.setColorAt(0, dim); grad.setColorAt(1, col);
	p.setBrush(grad);
	p.drawRoundedRect(x + 2, y + 2, fill_w, h - 4, 2, 2);
	p.setPen(QColor(180,180,200));
	p.setFont(QFont("sans-serif", 7, QFont::Bold));
	p.drawText(area, Qt::AlignLeft | Qt::AlignVCenter, QString("  %1").arg(label));
	p.setFont(QFont("monospace", 7));
	p.drawText(area, Qt::AlignRight | Qt::AlignVCenter,
		QString("%1  ").arg(val, 0, 'f', 2));
}
void GraphDetailDialog::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	int w = width(), h = height();
	int mg = 8; // margin
	int gap = 4;
	int time_h = 18;
	int fv_w = 36; // vertical fader width
	int fh_h = 22; // horizontal fader height

	// Count faders per side
	int n_left = (m_in_pos==FP_Left?1:0) + (m_out_pos==FP_Left?1:0);
	int n_right = (m_in_pos==FP_Right?1:0) + (m_out_pos==FP_Right?1:0);
	int n_top = (m_in_pos==FP_Top?1:0) + (m_out_pos==FP_Top?1:0);
	int n_bot = (m_in_pos==FP_Bottom?1:0) + (m_out_pos==FP_Bottom?1:0);

	// Space consumed by each side
	int left_sp = n_left * (fv_w + gap);
	int right_sp = n_right * (fv_w + gap);
	int top_sp = n_top * (fh_h + gap);
	int bot_sp = n_bot * (fh_h + gap);

	// Graph area
	int gx = mg + left_sp;
	int gy = mg + top_sp;
	int gw = w - gx - mg - right_sp;
	int gh = h - gy - mg - time_h - bot_sp;
	QRect ga(gx, gy, gw, gh);

	// Helper: place a fader on a side, returning its rect
	// slot=0 is closer to edge, slot=1 is inward
	auto vfader_rect = [&](FaderPos side, int slot) -> QRect {
		int fh = ga.height();
		int fy = ga.top();
		if (side == FP_Left) {
			int fx = mg + slot * (fv_w + gap);
			return QRect(fx, fy, fv_w, fh);
		} else { // FP_Right
			int fx = ga.right() + gap + slot * (fv_w + gap);
			return QRect(fx, fy, fv_w, fh);
		}
	};
	auto hfader_rect = [&](FaderPos side, int slot) -> QRect {
		int fw_full = ga.width();
		int fx = ga.left();
		if (side == FP_Top) {
			int fy = mg + slot * (fh_h + gap);
			return QRect(fx, fy, fw_full, fh_h);
		} else { // FP_Bottom
			int fy = ga.bottom() + gap + slot * (fh_h + gap);
			return QRect(fx, fy, fw_full, fh_h);
		}
	};

	// Build fader draw list: (rect, val, color, label, is_vertical)
	struct FaderInfo { QRect r; double val; QColor col; QString lbl; bool vert; };
	QVector<FaderInfo> faders;

	// For each side, place IN before OUT (IN gets slot 0 = closer to edge / on top)
	auto place_side = [&](FaderPos side) {
		bool is_v = (side == FP_Left || side == FP_Right);
		bool in_here = (m_in_pos == side);
		bool out_here = (m_out_pos == side);
		int slot = 0;
		if (in_here) {
			QRect r = is_v ? vfader_rect(side, slot) : hfader_rect(side, slot);
			faders.append({r, m_last_secondary, m_secondary_color, "IN", is_v});
			slot++;
		}
		if (out_here) {
			QRect r = is_v ? vfader_rect(side, slot) : hfader_rect(side, slot);
			faders.append({r, m_last_primary, m_primary_color, "OUT", is_v});
		}
	};
	place_side(FP_Left); place_side(FP_Right);
	place_side(FP_Top); place_side(FP_Bottom);

	// Graph background
	p.setPen(Qt::NoPen); p.setBrush(QColor(18,18,26));
	p.drawRoundedRect(ga.adjusted(-2,-2,2,2), 4, 4);

	// Horizontal grid lines
	p.setPen(QPen(QColor(50,50,60), 0.5, Qt::DotLine));
	for (int i = 1; i < 4; i++) {
		int ly = ga.top() + i * ga.height() / 4;
		p.drawLine(ga.left(), ly, ga.right(), ly);
	}

	// Time markers
	int count = m_full ? BUF_SIZE : m_head;
	double total_secs = count * 0.016;
	p.setFont(QFont("sans-serif", 7));
	int time_labels = qMin(5, qMax(2, int(total_secs)));
	for (int t = 0; t <= time_labels; t++) {
		double frac = double(t) / time_labels;
		int tx = ga.left() + int(frac * ga.width());
		double secs = -total_secs * (1.0 - frac);
		QString lbl = (t == time_labels) ? "now"
			: QString("%1s").arg(secs, 0, 'f', 1);
		p.setPen(QPen(QColor(45,45,55), 0.5, Qt::DotLine));
		p.drawLine(tx, ga.top(), tx, ga.bottom());
		p.setPen(QColor(100,100,120));
		p.drawText(QRect(tx - 20, ga.bottom() + 2, 40, 14),
			Qt::AlignCenter, lbl);
	}

	// Value scale labels
	p.setPen(QColor(80,80,100));
	p.setFont(QFont("monospace", 6));
	double range = m_max - m_min;
	for (int i = 0; i <= 4; i++) {
		double v = m_max - i * range / 4.0;
		int ly = ga.top() + i * ga.height() / 4;
		p.drawText(QRect(ga.right() + 2, ly - 6, 30, 12),
			Qt::AlignLeft | Qt::AlignVCenter, QString::number(v, 'f', 1));
	}

	// Draw series
	QColor dim_sec = m_secondary_color; dim_sec.setAlpha(140);
	draw_series(p, m_secondary, m_head, m_full, dim_sec, ga);
	draw_series(p, m_primary, m_head, m_full, m_primary_color, ga);

	// Draw faders
	for (auto &f : faders) {
		if (f.vert) draw_fader_v(p, f.r, f.val, f.col, f.lbl);
		else        draw_fader_h(p, f.r, f.val, f.col, f.lbl);
	}
}
void GraphDetailDialog::contextMenuEvent(QContextMenuEvent *e) {
	QMenu menu(this);
	auto build_sub = [&](const QString &name, FaderPos &pos_ref) {
		auto *sub = menu.addMenu(name);
		auto add = [&](const QString &lbl, FaderPos fp) {
			auto *act = sub->addAction(lbl);
			act->setCheckable(true);
			act->setChecked(pos_ref == fp);
			connect(act, &QAction::triggered, this, [this, &pos_ref, fp]{
				pos_ref = fp; update();
			});
		};
		add("Hide", FP_Hidden);
		sub->addSeparator();
		add("Left", FP_Left);
		add("Right", FP_Right);
		add("Top", FP_Top);
		add("Bottom", FP_Bottom);
	};
	build_sub("IN Fader", m_in_pos);
	build_sub("OUT Fader", m_out_pos);
	menu.exec(e->globalPos());
}

// ===== MiniGraph ==========================================================
MiniGraph::MiniGraph(const QColor &line_color, int sample_count,
	double val_min, double val_max, QWidget *parent)
	: QWidget(parent), m_line_color(line_color), m_line_color_b(Qt::gray),
	  m_sample_count(sample_count), m_min(val_min), m_max(val_max)
{
	m_samples.resize(sample_count, 0.0);
	m_samples_b.resize(sample_count, 0.0);
	setFixedHeight(32);
	setMinimumWidth(60);
	setStyleSheet("background:rgba(20,20,30,180);border-radius:3px;");
}
void MiniGraph::push(double val) {
	m_samples[m_head] = val;
	if (m_dual) m_samples_b[m_head] = val;
	forward_to_detail(val, m_dual ? val : val);
	m_head = (m_head + 1) % m_sample_count;
	if (m_head == 0) m_full = true;
	update();
}
void MiniGraph::push_dual(double val_a, double val_b) {
	m_samples[m_head] = val_a;
	m_samples_b[m_head] = val_b;
	m_dual = true;
	forward_to_detail(val_a, val_b);
	m_head = (m_head + 1) % m_sample_count;
	if (m_head == 0) m_full = true;
	update();
}
void MiniGraph::draw_series(QPainter &p, const QVector<double> &buf, int head, bool full, const QColor &col) {
	int w = width(), h = height();
	int count = full ? m_sample_count : head;
	if (count < 2) return;
	double range = (m_max == m_min) ? 1.0 : (m_max - m_min);
	QVector<QPointF> pts;
	for (int i = 0; i < count; i++) {
		int idx = full ? (head + i) % m_sample_count : i;
		double norm = qBound(0.0, (buf[idx] - m_min) / range, 1.0);
		double x = double(i) / (count - 1) * (w - 2) + 1;
		double y = (1.0 - norm) * (h - 4) + 2;
		pts.append(QPointF(x, y));
	}
	QColor fill_color = col; fill_color.setAlpha(30);
	QVector<QPointF> fill_pts = pts;
	fill_pts.append(QPointF(pts.last().x(), h));
	fill_pts.append(QPointF(pts.first().x(), h));
	p.setPen(Qt::NoPen); p.setBrush(fill_color);
	p.drawPolygon(fill_pts.data(), fill_pts.size());
	QPen pen(col, 1.2); p.setPen(pen); p.setBrush(Qt::NoBrush);
	p.drawPolyline(pts.data(), pts.size());
}
void MiniGraph::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	if (m_dimmed) {
		// Greyed out — draw flat line and dark overlay
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(30,30,38,180));
		p.drawRect(rect());
		p.setPen(QPen(QColor(80,80,90), 0.5, Qt::DashLine));
		p.drawLine(0, height()/2, width(), height()/2);
		return;
	}
	if (m_dual) {
		// Draw secondary (input) behind primary (output)
		QColor dim_b = m_line_color_b; dim_b.setAlpha(120);
		draw_series(p, m_samples_b, m_head, m_full, dim_b);
	}
	draw_series(p, m_samples, m_head, m_full, m_line_color);
}
void MiniGraph::forward_to_detail(double a, double b) {
	if (m_detail) m_detail->push(a, b);
}
void MiniGraph::mouseDoubleClickEvent(QMouseEvent *) {
	if (m_detail) { m_detail->raise(); m_detail->activateWindow(); return; }
	QString title = m_title.isEmpty() ? "Signal" : m_title;
	QColor sec = m_dual ? m_line_color_b : m_line_color;
	m_detail = new GraphDetailDialog(title, m_line_color, sec, m_min, m_max,
		window());
	m_detail->show();
}
// ===== StageRow (base) ====================================================
StageRow::StageRow(int index, const QColor &dot_color, QWidget *parent)
	: QWidget(parent), m_index(index)
{
	m_dot = new ActivityDot(dot_color, this);
	m_enabled = new QCheckBox(this); m_enabled->setChecked(true);
	m_type = new QComboBox(this); m_type->setFixedWidth(100);
	m_p1_label = new QLabel("P1:",this); m_p1_label->setFixedWidth(22);
	m_p1 = new QDoubleSpinBox(this); m_p1->setRange(-10000,10000); m_p1->setDecimals(3); m_p1->setFixedWidth(70);
	m_p2_label = new QLabel("P2:",this); m_p2_label->setFixedWidth(22);
	m_p2 = new QDoubleSpinBox(this); m_p2->setRange(-10000,10000); m_p2->setDecimals(3); m_p2->setFixedWidth(70);
	m_preview = new QLabel("—",this); m_preview->setFixedWidth(90); m_preview->setAlignment(Qt::AlignCenter);
	m_preview->setStyleSheet("color:#7cf;font-size:10px;background:rgba(30,30,40,150);border-radius:3px;padding:1px 3px;");
	m_graph = new MiniGraph(dot_color, 60, 0.0, 1.0, this);
	QColor dim_in = dot_color; dim_in.setAlpha(100);
	m_graph->set_secondary_color(dim_in);
	m_graph->setFixedSize(60, 28);
}
void StageRow::setup_base_row(QHBoxLayout *row) {
	row->setContentsMargins(2,1,2,1); row->setSpacing(3);
	row->addWidget(m_dot); row->addWidget(m_enabled); row->addWidget(m_type);
	row->addWidget(m_p1_label); row->addWidget(m_p1);
	row->addWidget(m_p2_label); row->addWidget(m_p2);
	row->addWidget(m_preview);
	row->addWidget(m_graph);
	auto *up=new QPushButton("▲",this); up->setFixedSize(18,18);
	auto *dn=new QPushButton("▼",this); dn->setFixedSize(18,18);
	auto *rm=new QPushButton("✕",this); rm->setFixedSize(18,18); rm->setStyleSheet("color:#e74c3c;");
	row->addWidget(up); row->addWidget(dn); row->addWidget(rm);
	connect(up,&QPushButton::clicked,this,[this]{emit move_up(m_index);});
	connect(dn,&QPushButton::clicked,this,[this]{emit move_down(m_index);});
	connect(rm,&QPushButton::clicked,this,[this]{emit remove(m_index);});
	connect(m_enabled,&QCheckBox::toggled,this,[this](bool on){
		m_graph->set_dimmed(!on);
		m_dot->setVisible(on);
		if (!on) m_preview->setText("—");
		emit changed();
	});
	connect(m_p1,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
	connect(m_p2,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
}
void StageRow::set_preview(double in, double out) {
	bool on = m_enabled->isChecked();
	m_graph->set_dimmed(!on);
	if (!on) {
		m_preview->setText("—");
		return;
	}
	m_preview->setText(QString("%1→%2").arg(in,0,'f',2).arg(out,0,'f',2));
	m_graph->push_dual(out, in);
}
void StageRow::set_index(int idx) { m_index = idx; }
void StageRow::pulse_activity() { if (m_enabled->isChecked()) m_dot->pulse(); }
bool StageRow::is_stage_enabled() const { return m_enabled->isChecked(); }

// ===== InterpStageRow =====================================================
InterpStageRow::InterpStageRow(int index, QWidget *parent)
	: StageRow(index, QColor(140,120,255), parent)
{
	m_type->addItem("Linear",0); m_type->addItem("Quantize",1);
	m_type->addItem("Smooth",2); m_type->addItem("S-Curve",3);
	m_type->addItem("Easing",4); m_type->addItem("Animate To",5);
	m_type->addItem("Animate From",6);
	auto *row = new QHBoxLayout(this);
	setup_base_row(row);
	connect(m_type,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&InterpStageRow::on_type_changed);
	on_type_changed(0);
}
void InterpStageRow::on_type_changed(int) {
	int t = m_type->currentData().toInt();
	bool s1=false,s2=false;
	switch(t){
	case InterpStage::Quantize: s1=true; m_p1_label->setText("%:"); m_p1->setRange(1,100); m_p1->setDecimals(0); m_p1->setSingleStep(1);
		if(m_p1->value()<1) m_p1->setValue(10); break;
	case InterpStage::Smooth: s1=true; m_p1_label->setText("%:"); m_p1->setRange(1,100); m_p1->setDecimals(0); m_p1->setSingleStep(5);
		if(m_p1->value()<1) m_p1->setValue(30); break;
	case InterpStage::Easing: s1=true; m_p1_label->setText("Crv:"); m_p1->setRange(0,40); m_p1->setDecimals(0); break;
	case InterpStage::AnimateTo: case InterpStage::AnimateFrom:
		s1=s2=true; m_p1_label->setText("ms:"); m_p1->setRange(10,10000); m_p1->setDecimals(0); if(m_p1->value()<10)m_p1->setValue(500);
		m_p2_label->setText("Eas:"); m_p2->setRange(0,40); m_p2->setDecimals(0); break;
	default: break;
	}
	m_p1_label->setVisible(s1); m_p1->setVisible(s1);
	m_p2_label->setVisible(s2); m_p2->setVisible(s2);
	emit changed();
}
void InterpStageRow::load(const InterpStage &s) {
	m_enabled->setChecked(s.enabled);
	int idx=m_type->findData(s.type); if(idx>=0)m_type->setCurrentIndex(idx);
	// Quantize/Smooth: internal 0-1 → display 0-100
	if (s.type == InterpStage::Quantize || s.type == InterpStage::Smooth)
		m_p1->setValue(s.param1 * 100.0);
	else
		m_p1->setValue(s.param1);
	m_p2->setValue(s.param2);
}
InterpStage InterpStageRow::build() const {
	InterpStage s; s.type=m_type->currentData().toInt();
	s.enabled=m_enabled->isChecked();
	// Quantize/Smooth: display 0-100 → internal 0-1
	if (s.type == InterpStage::Quantize || s.type == InterpStage::Smooth)
		s.param1 = m_p1->value() / 100.0;
	else
		s.param1 = m_p1->value();
	s.param2 = m_p2->value();
	return s;
}

// ===== FilterStageRow =====================================================
FilterStageRow::FilterStageRow(int index, const QColor &color, QWidget *parent)
	: StageRow(index, color, parent)
{
	m_type->addItem("Delay",0); m_type->addItem("Debounce",1);
	m_type->addItem("Rate Limit",2); m_type->addItem("Deadzone",3);
	m_type->addItem("Clamp",4); m_type->addItem("Scale",5);
	m_type->setFixedWidth(90);
	auto *row = new QHBoxLayout(this);
	setup_base_row(row);
	connect(m_type,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&FilterStageRow::on_type_changed);
	on_type_changed(0);
}
void FilterStageRow::on_type_changed(int) {
	int t=m_type->currentData().toInt();
	bool s1=true,s2=false;
	switch(t){
	case FilterStage::Delay: m_p1_label->setText("ms:"); m_p1->setRange(0,5000); m_p1->setDecimals(0); break;
	case FilterStage::Debounce: m_p1_label->setText("ms:"); m_p1->setRange(0,5000); m_p1->setDecimals(0); break;
	case FilterStage::RateLimit: m_p1_label->setText("/s:"); m_p1->setRange(0.1,10000); m_p1->setDecimals(1); break;
	case FilterStage::Deadzone: m_p1_label->setText("Thr:"); m_p1->setRange(0,1000); m_p1->setDecimals(2); break;
	case FilterStage::Clamp: m_p1_label->setText("Min:"); m_p2_label->setText("Max:"); s2=true;
		if(m_p1->value()==0.0 && m_p2->value()==0.0){ m_p1->setValue(0.0); m_p2->setValue(127.0); } break;
	case FilterStage::Scale: m_p1_label->setText("×:"); m_p2_label->setText("+:"); s2=true;
		if(m_p1->value()==0.0) m_p1->setValue(1.0); break;
	}
	m_p1_label->setVisible(s1); m_p1->setVisible(s1);
	m_p2_label->setVisible(s2); m_p2->setVisible(s2);
	emit changed();
}
void FilterStageRow::load(const FilterStage &s) {
	m_enabled->setChecked(s.enabled);
	int idx=m_type->findData(s.type); if(idx>=0)m_type->setCurrentIndex(idx);
	m_p1->setValue(s.param1); m_p2->setValue(s.param2);
}
FilterStage FilterStageRow::build() const {
	FilterStage s; s.type=m_type->currentData().toInt();
	s.enabled=m_enabled->isChecked(); s.param1=m_p1->value(); s.param2=m_p2->value();
	return s;
}

// ===== MasterPreview ======================================================
MasterPreview::MasterPreview(const QString &name, double min, double max, QWidget *parent)
	: QWidget(parent), m_min(min), m_max(max)
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(8,6,8,6); outer->setSpacing(4);
	// Top row: dot, name, raw, value, meter
	auto *top = new QHBoxLayout();
	top->setSpacing(8);
	m_input_dot = new ActivityDot(QColor(100,180,255), this);
	m_name_label = new QLabel(QString("<b style='color:#8af;'>⚡ %1</b>").arg(name), this);
	m_value_label = new QLabel("0.000", this);
	m_value_label->setStyleSheet("color:#fff;font-size:16px;font-weight:bold;font-family:monospace;");
	m_value_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
	m_value_label->setMinimumWidth(80);
	m_raw_label = new QLabel("MIDI: —", this);
	m_raw_label->setStyleSheet("color:#888;font-size:10px;");
	m_meter = new QProgressBar(this);
	m_meter->setRange(0, 1000); m_meter->setValue(0);
	m_meter->setTextVisible(false); m_meter->setFixedHeight(6);
	m_meter->setStyleSheet("QProgressBar{background:rgba(40,40,55,200);border:none;border-radius:3px;}"
		"QProgressBar::chunk{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #4a8af4,stop:1 #7cf);border-radius:3px;}");
	top->addWidget(m_input_dot);
	top->addWidget(m_name_label);
	top->addWidget(m_raw_label);
	top->addStretch();
	top->addWidget(m_value_label);
	auto *right = new QVBoxLayout();
	right->addWidget(m_meter);
	top->addLayout(right);
	outer->addLayout(top);
	// Single overlaid graph — MIDI In (blue, behind) + Ctrl Out (cyan, front)
	m_graph = new MiniGraph(QColor(100,220,180), 120, min, max, this);
	m_graph->set_secondary_color(QColor(80,140,220));
	m_graph->set_title(name);
	m_graph->setFixedHeight(40);
	outer->addWidget(m_graph);
	setStyleSheet("background:rgba(35,35,50,200);border-radius:6px;");
}
void MasterPreview::set_value(double val) {
	m_value_label->setText(QString::number(val,'f',3));
	double norm = (m_max==m_min) ? 0 : qBound(0.0,(val-m_min)/(m_max-m_min),1.0);
	m_meter->setValue(int(norm*1000));
	// Push dual: primary = output, secondary = raw input (both in output range)
	m_graph->push_dual(val, m_last_raw_norm);
}
void MasterPreview::pulse_input() { m_input_dot->pulse(); }
void MasterPreview::set_raw_midi(int raw) {
	m_raw_label->setText(QString("MIDI: %1").arg(raw));
	// Normalize raw 0-127 into output range for overlay
	m_last_raw_norm = m_min + (raw / 127.0) * (m_max - m_min);
}

// ===== OutputBindingPanel =================================================
OutputBindingPanel::OutputBindingPanel(int index, QWidget *parent)
	: QFrame(parent), m_index(index)
{
	setFrameShape(QFrame::StyledPanel);
	auto *top = new QVBoxLayout(this); top->setContentsMargins(4,4,4,4); top->setSpacing(2);
	m_header_btn = new QPushButton(QString("Output #%1 ▶").arg(index+1),this);
	m_header_btn->setFlat(true); m_header_btn->setStyleSheet("text-align:left;font-weight:bold;padding:4px;");
	top->addWidget(m_header_btn);
	m_body = new QWidget(this); m_body->setVisible(false);
	auto *form = new QFormLayout(m_body); form->setContentsMargins(8,4,8,4); form->setSpacing(3);
	m_enabled = new QCheckBox("Enabled",m_body); m_enabled->setChecked(true); form->addRow("",m_enabled);
	m_device_combo = new QComboBox(m_body); form->addRow("Device:",m_device_combo);
	m_channel_spin = new QSpinBox(m_body); m_channel_spin->setRange(0,15); form->addRow("Channel:",m_channel_spin);
	m_cc_spin = new QSpinBox(m_body); m_cc_spin->setRange(0,127); form->addRow("CC:",m_cc_spin);
	m_in_min_spin = new QDoubleSpinBox(m_body); m_in_min_spin->setRange(-9999,9999); m_in_min_spin->setDecimals(2); form->addRow("Port Min:",m_in_min_spin);
	m_in_max_spin = new QDoubleSpinBox(m_body); m_in_max_spin->setRange(-9999,9999); m_in_max_spin->setDecimals(2); m_in_max_spin->setValue(1.0); form->addRow("Port Max:",m_in_max_spin);
	m_out_min_spin = new QSpinBox(m_body); m_out_min_spin->setRange(0,127); form->addRow("MIDI Min:",m_out_min_spin);
	m_out_max_spin = new QSpinBox(m_body); m_out_max_spin->setRange(0,127); m_out_max_spin->setValue(127); form->addRow("MIDI Max:",m_out_max_spin);
	m_on_change_check = new QCheckBox("Only on change",m_body); m_on_change_check->setChecked(true);
	form->addRow("",m_on_change_check);
	auto *rm=new QPushButton("Remove",m_body); rm->setStyleSheet("color:#e74c3c;"); form->addRow("",rm);
	top->addWidget(m_body);
	connect(m_header_btn,&QPushButton::clicked,this,[this]{emit expand_requested(m_index);});
	connect(rm,&QPushButton::clicked,this,[this]{emit remove_requested(m_index);});
	auto sig=[this]{emit changed();};
	connect(m_enabled,&QCheckBox::toggled,this,sig); connect(m_on_change_check,&QCheckBox::toggled,this,sig);
}
void OutputBindingPanel::load(const MidiOutputBinding &o) {
	m_enabled->setChecked(o.enabled);
	if(o.device_index>=0&&o.device_index<m_device_combo->count()) m_device_combo->setCurrentIndex(o.device_index);
	m_channel_spin->setValue(o.channel); m_cc_spin->setValue(o.cc);
	m_in_min_spin->setValue(o.input_min); m_in_max_spin->setValue(o.input_max);
	m_out_min_spin->setValue(o.output_min); m_out_max_spin->setValue(o.output_max);
	m_on_change_check->setChecked(o.on_change);
}
MidiOutputBinding OutputBindingPanel::build(const QString &port_id) const {
	MidiOutputBinding o; o.port_id=port_id; o.enabled=m_enabled->isChecked();
	o.device_index=m_device_combo->currentIndex(); o.channel=m_channel_spin->value(); o.cc=m_cc_spin->value();
	o.input_min=m_in_min_spin->value(); o.input_max=m_in_max_spin->value();
	o.output_min=m_out_min_spin->value(); o.output_max=m_out_max_spin->value();
	o.on_change=m_on_change_check->isChecked(); return o;
}
void OutputBindingPanel::populate_devices(const QStringList &d) { m_device_combo->clear(); m_device_combo->addItems(d); }
void OutputBindingPanel::set_index(int i) { m_index=i; m_header_btn->setText(QString("Output #%1 %2").arg(i+1).arg(m_expanded?"▼":"▶")); }
void OutputBindingPanel::set_expanded(bool e) { m_expanded=e; m_body->setVisible(e); set_index(m_index); }
bool OutputBindingPanel::is_expanded() const { return m_expanded; }

} // namespace super
