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
	// Rebuild columns if stage count or names changed
	int n_pre = p.after_pre_filter.size();
	int n_int = p.after_interp.size();
	int n_post = p.after_post_filter.size();
	// total = pre + Norm + interp + Map + post
	int total = n_pre + 1 + n_int + 1 + n_post;
	QStringList all_names = p.pre_filter_names + p.interp_names + p.post_filter_names;
	QString name_key = all_names.join("|");
	if (total != m_prev_col_count || name_key != m_prev_name_key) {
		m_columns.clear();
		auto mk = [&](const QString &l, const QColor &col, double mn, double mx) {
			Column c; c.label = l; c.color = col;
			c.val_min = mn; c.val_max = mx;
			c.buf_in.resize(COL_BUF, 0); c.buf_out.resize(COL_BUF, 0);
			m_columns.append(c);
		};
		for (int i = 0; i < n_pre; i++) {
			QString name = (i < p.pre_filter_names.size()) ? p.pre_filter_names[i] : "";
			mk(QString("Pre #%1\n%2").arg(i+1).arg(name), QColor(46,204,113), 0, 127);
		}
		mk(QString("Norm\n%1-%2\u21920-1").arg(p.input_min).arg(p.input_max),
			QColor(180,140,255), 0, 127);
		for (int i = 0; i < n_int; i++) {
			QString name = (i < p.interp_names.size()) ? p.interp_names[i] : "";
			mk(QString("Interp #%1\n%2").arg(i+1).arg(name), QColor(52,152,219), 0, 1.0);
		}
		mk(QString("Map\n0-1\u2192%1-%2")
			.arg(p.output_min,0,'f',1).arg(p.output_max,0,'f',1),
			QColor(255,180,80), m_out_min, m_out_max);
		for (int i = 0; i < n_post; i++) {
			QString name = (i < p.post_filter_names.size()) ? p.post_filter_names[i] : "";
			mk(QString("Post #%1\n%2").arg(i+1).arg(name), QColor(230,126,34), m_out_min, m_out_max);
		}
		m_prev_col_count = total;
		m_prev_name_key = name_key;
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

void PipelineVisualDialog::set_static(int raw, const PipelinePreview &p) {
	// Rebuild columns if stage count or names changed (same logic as feed)
	int n_pre = p.after_pre_filter.size();
	int n_int = p.after_interp.size();
	int n_post = p.after_post_filter.size();
	int total = n_pre + 1 + n_int + 1 + n_post;
	QStringList all_names = p.pre_filter_names + p.interp_names + p.post_filter_names;
	QString name_key = all_names.join("|");
	if (total != m_prev_col_count || name_key != m_prev_name_key) {
		m_columns.clear();
		auto mk = [&](const QString &l, const QColor &col, double mn, double mx) {
			Column c; c.label = l; c.color = col;
			c.val_min = mn; c.val_max = mx;
			c.buf_in.resize(COL_BUF, 0); c.buf_out.resize(COL_BUF, 0);
			m_columns.append(c);
		};
		for (int i = 0; i < n_pre; i++) {
			QString name = (i < p.pre_filter_names.size()) ? p.pre_filter_names[i] : "";
			mk(QString("Pre #%1\n%2").arg(i+1).arg(name), QColor(46,204,113), 0, 127);
		}
		mk(QString("Norm\n%1-%2\u21920-1").arg(p.input_min).arg(p.input_max),
			QColor(180,140,255), 0, 127);
		for (int i = 0; i < n_int; i++) {
			QString name = (i < p.interp_names.size()) ? p.interp_names[i] : "";
			mk(QString("Interp #%1\n%2").arg(i+1).arg(name), QColor(52,152,219), 0, 1.0);
		}
		mk(QString("Map\n0-1\u2192%1-%2")
			.arg(p.output_min,0,'f',1).arg(p.output_max,0,'f',1),
			QColor(255,180,80), m_out_min, m_out_max);
		for (int i = 0; i < n_post; i++) {
			QString name = (i < p.post_filter_names.size()) ? p.post_filter_names[i] : "";
			mk(QString("Post #%1\n%2").arg(i+1).arg(name), QColor(230,126,34), m_out_min, m_out_max);
		}
		m_prev_col_count = total;
		m_prev_name_key = name_key;
	}
	// Set static values — no ring buffer push
	int ci = 0;
	for (int i = 0; i < n_pre; i++, ci++) {
		auto &c = m_columns[ci];
		c.last_in = (i==0) ? double(raw) : p.after_pre_filter[i-1];
		c.last_out = p.after_pre_filter[i];
		c.dimmed = (i < p.pre_filter_enabled.size()) && !p.pre_filter_enabled[i];
	}
	{ auto &c = m_columns[ci++];
		c.last_in = p.pre_filtered;
		c.last_out = p.normalized * 127.0;
	}
	for (int i = 0; i < n_int; i++, ci++) {
		auto &c = m_columns[ci];
		c.last_in = (i==0) ? p.normalized : p.after_interp[i-1];
		c.last_out = p.after_interp[i];
		c.dimmed = (i < p.interp_enabled.size()) && !p.interp_enabled[i];
	}
	{ auto &c = m_columns[ci++];
		double interp_last = n_int > 0 ? p.after_interp.last() : p.normalized;
		double range = m_out_max - m_out_min;
		c.last_in = m_out_min + interp_last * range;
		c.last_out = p.mapped;
	}
	for (int i = 0; i < n_post; i++, ci++) {
		auto &c = m_columns[ci];
		c.last_in = (i==0) ? p.mapped : p.after_post_filter[i-1];
		c.last_out = p.after_post_filter[i];
		c.dimmed = (i < p.post_filter_enabled.size()) && !p.post_filter_enabled[i];
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
			// Grid (faint)
			p.setPen(QPen(QColor(35,35,45), 0.5, Qt::DotLine));
			p.drawLine(col_area.left(), col_area.top()+graph_h/2,
				col_area.right(), col_area.top()+graph_h/2);
			// Draw series in grey
			Column grey_c = c;
			grey_c.color = QColor(65,65,75);
			draw_col_graph(p, col_area, grey_c);
			// Dark overlay
			p.setPen(Qt::NoPen); p.setBrush(QColor(22,22,30,140));
			p.drawRoundedRect(col_area, 3, 3);
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
void GraphDetailDialog::seed(const QVector<double> &pri, const QVector<double> &sec,
	int head, bool full, double last_pri, double last_sec)
{
	// Copy MiniGraph buffers into our larger buffer, resampling if sizes differ
	int src_count = full ? pri.size() : head;
	if (src_count < 1) return;
	m_head = 0; m_full = false;
	for (int i = 0; i < src_count; i++) {
		int src_idx = full ? (head + i) % pri.size() : i;
		m_primary[m_head] = pri[src_idx];
		m_secondary[m_head] = (src_idx < sec.size()) ? sec[src_idx] : 0.0;
		m_head = (m_head + 1) % BUF_SIZE;
		if (m_head == 0) m_full = true;
	}
	m_last_primary = last_pri;
	m_last_secondary = last_sec;
	update();
}
void GraphDetailDialog::draw_linear_ref(QPainter &p, const QRect &area) {
	// Dimmed diagonal "identity" line when no data
	QPen pen(QColor(60,60,80), 1.0, Qt::DashLine);
	p.setPen(pen); p.setBrush(Qt::NoBrush);
	p.drawLine(area.bottomLeft(), area.topRight());
}
void GraphDetailDialog::draw_series(QPainter &p, const QVector<double> &buf,
	int head, bool full, const QColor &col, const QRect &area)
{
	int count = full ? BUF_SIZE : head;
	if (count < 2) {
		draw_linear_ref(p, area);
		return;
	}
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
		// Greyed out — draw existing series desaturated, then dark overlay
		QColor grey(70, 70, 80);
		if (m_full || m_head > 1) {
			if (m_dual) draw_series(p, m_samples_b, m_head, m_full, QColor(55,55,65));
			draw_series(p, m_samples, m_head, m_full, grey);
		} else {
			p.setPen(QPen(grey, 0.5, Qt::DashLine));
			p.drawLine(QPoint(0, height()), QPoint(width(), 0));
		}
		// Dark overlay
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(22,22,30,140));
		p.drawRect(rect());
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
void MiniGraph::close_detail() {
	if (m_detail) { m_detail->close(); m_detail = nullptr; }
}
void MiniGraph::mouseDoubleClickEvent(QMouseEvent *) {
	if (m_detail) { m_detail->raise(); m_detail->activateWindow(); return; }
	QString title = m_title.isEmpty() ? "Signal" : m_title;
	QColor sec = m_dual ? m_line_color_b : m_line_color;
	m_detail = new GraphDetailDialog(title, m_line_color, sec, m_min, m_max,
		window());
	// Seed with existing buffer data so graph isn't empty on open
	int last_idx = (m_head > 0) ? m_head - 1 : (m_full ? m_sample_count - 1 : 0);
	double last_a = (m_full || m_head > 0) ? m_samples[last_idx] : 0.0;
	double last_b = m_dual && (m_full || m_head > 0) ? m_samples_b[last_idx] : 0.0;
	m_detail->seed(m_samples, m_dual ? m_samples_b : m_samples,
		m_head, m_full, last_a, last_b);
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
StageRow::~StageRow() {
	m_graph->close_detail();
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
void StageRow::set_preview_label(double in, double out) {
	bool on = m_enabled->isChecked();
	m_graph->set_dimmed(!on);
	if (!on) { m_preview->setText("—"); return; }
	m_preview->setText(QString("%1→%2").arg(in,0,'f',2).arg(out,0,'f',2));
	// No graph push, no dot pulse
}
void StageRow::set_index(int idx) { m_index = idx; }
void StageRow::pulse_activity() { if (m_enabled->isChecked()) m_dot->pulse(); }
bool StageRow::is_stage_enabled() const { return m_enabled->isChecked(); }
void StageRow::update_title(const QString &prefix, int num) {
	QString pfx = m_title_prefix.isEmpty() ? prefix : m_title_prefix;
	QString type_name = m_type->currentText();
	m_graph->set_title(QString("%1 #%2 — %3").arg(pfx).arg(num).arg(type_name));
}

// ===== InterpStageRow =====================================================
InterpStageRow::InterpStageRow(int index, QWidget *parent)
	: StageRow(index, QColor(140,120,255), parent)
{
	m_type->addItem("Linear",0); m_type->addItem("Quantize",1);
	m_type->addItem("Smooth",2); m_type->addItem("S-Curve",3);
	m_type->addItem("Easing",4);

	// Named easing curve combo (hidden until Easing type is selected)
	m_easing_combo = new QComboBox(this);
	m_easing_combo->setFixedWidth(110);
	m_easing_combo->addItem("Linear",        (int)QEasingCurve::Linear);
	m_easing_combo->addItem("InQuad",        (int)QEasingCurve::InQuad);
	m_easing_combo->addItem("OutQuad",       (int)QEasingCurve::OutQuad);
	m_easing_combo->addItem("InOutQuad",     (int)QEasingCurve::InOutQuad);
	m_easing_combo->addItem("InCubic",       (int)QEasingCurve::InCubic);
	m_easing_combo->addItem("OutCubic",      (int)QEasingCurve::OutCubic);
	m_easing_combo->addItem("InOutCubic",    (int)QEasingCurve::InOutCubic);
	m_easing_combo->addItem("InExpo",        (int)QEasingCurve::InExpo);
	m_easing_combo->addItem("OutExpo",       (int)QEasingCurve::OutExpo);
	m_easing_combo->addItem("InOutExpo",     (int)QEasingCurve::InOutExpo);
	m_easing_combo->addItem("InBounce",      (int)QEasingCurve::InBounce);
	m_easing_combo->addItem("OutBounce",     (int)QEasingCurve::OutBounce);
	m_easing_combo->addItem("InOutBounce",   (int)QEasingCurve::InOutBounce);
	m_easing_combo->addItem("InElastic",     (int)QEasingCurve::InElastic);
	m_easing_combo->addItem("OutElastic",    (int)QEasingCurve::OutElastic);
	m_easing_combo->addItem("InOutElastic",  (int)QEasingCurve::InOutElastic);
	m_easing_combo->addItem("InBack",        (int)QEasingCurve::InBack);
	m_easing_combo->addItem("OutBack",       (int)QEasingCurve::OutBack);
	m_easing_combo->addItem("InOutBack",     (int)QEasingCurve::InOutBack);
	m_easing_combo->addItem("InSine",        (int)QEasingCurve::InSine);
	m_easing_combo->addItem("OutSine",       (int)QEasingCurve::OutSine);
	m_easing_combo->addItem("InOutSine",     (int)QEasingCurve::InOutSine);
	m_easing_combo->addItem("InCirc",        (int)QEasingCurve::InCirc);
	m_easing_combo->addItem("OutCirc",       (int)QEasingCurve::OutCirc);
	m_easing_combo->addItem("InOutCirc",     (int)QEasingCurve::InOutCirc);
	m_easing_combo->setVisible(false);
	connect(m_easing_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ emit changed(); });

	auto *row = new QHBoxLayout(this);
	setup_base_row(row);
	row->insertWidget(row->count()-1, m_easing_combo); // Before preview
	connect(m_type,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&InterpStageRow::on_type_changed);
	on_type_changed(0);
}
void InterpStageRow::on_type_changed(int) {
	int t = m_type->currentData().toInt();
	bool s1=false,s2=false,easing=false;
	switch(t){
	case InterpStage::Quantize: s1=true; m_p1_label->setText("%:"); m_p1->setRange(1,100); m_p1->setDecimals(0); m_p1->setSingleStep(1);
		if(m_p1->value()<1) m_p1->setValue(10); break;
	case InterpStage::Smooth: s1=true; m_p1_label->setText("%:"); m_p1->setRange(1,100); m_p1->setDecimals(0); m_p1->setSingleStep(5);
		if(m_p1->value()<1) m_p1->setValue(30); break;
	case InterpStage::Easing: easing=true; break;
	default: break;
	}
	m_p1_label->setVisible(s1); m_p1->setVisible(s1);
	m_p2_label->setVisible(s2); m_p2->setVisible(s2);
	m_easing_combo->setVisible(easing);
	update_title("Interp", m_index + 1);
	emit changed();
}
void InterpStageRow::load(const InterpStage &s) {
	m_enabled->setChecked(s.enabled);
	int idx=m_type->findData(s.type); if(idx>=0)m_type->setCurrentIndex(idx);
	// Quantize/Smooth: internal 0-1 → display 0-100
	if (s.type == InterpStage::Quantize || s.type == InterpStage::Smooth)
		m_p1->setValue(s.param1 * 100.0);
	else if (s.type == InterpStage::Easing) {
		int ei = m_easing_combo->findData(static_cast<int>(s.param1));
		if (ei >= 0) m_easing_combo->setCurrentIndex(ei);
	} else
		m_p1->setValue(s.param1);
	m_p2->setValue(s.param2);
}
InterpStage InterpStageRow::build() const {
	InterpStage s; s.type=m_type->currentData().toInt();
	s.enabled=m_enabled->isChecked();
	// Quantize/Smooth: display 0-100 → internal 0-1
	if (s.type == InterpStage::Quantize || s.type == InterpStage::Smooth)
		s.param1 = m_p1->value() / 100.0;
	else if (s.type == InterpStage::Easing)
		s.param1 = static_cast<double>(m_easing_combo->currentData().toInt());
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
	update_title(m_title_prefix, m_index + 1);
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
	outer->setContentsMargins(10,8,10,8); outer->setSpacing(4);
	// Top row: dot, name, raw
	auto *top = new QHBoxLayout();
	top->setSpacing(6);
	m_input_dot = new ActivityDot(QColor(100,180,255), this);
	m_name_label = new QLabel(QString("<b style='color:#8af;'>⚡ %1</b>").arg(name), this);
	m_raw_label = new QLabel("MIDI: —", this);
	m_raw_label->setStyleSheet("color:#666;font-size:10px;");
	top->addWidget(m_input_dot);
	top->addWidget(m_name_label);
	top->addWidget(m_raw_label);
	top->addStretch();
	// Output dot + value label
	m_output_dot = new ActivityDot(QColor(100,220,180), this);
	m_value_label = new QLabel("0.000", this);
	m_value_label->setStyleSheet(
		"color:#fff;font-size:14px;font-weight:bold;font-family:'Consolas','Courier New',monospace;"
		"background:rgba(25,25,40,200);border:1px solid rgba(100,220,180,60);"
		"border-radius:4px;padding:2px 8px;");
	m_value_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
	m_value_label->setMinimumWidth(90);
	top->addWidget(m_output_dot);
	top->addWidget(m_value_label);
	// Pipeline button placeholder (added via add_pipeline_button)
	m_pipeline_btn_slot = new QHBoxLayout();
	top->addLayout(m_pipeline_btn_slot);
	outer->addLayout(top);
	// Meter bar
	m_meter = new QProgressBar(this);
	m_meter->setRange(0, 1000); m_meter->setValue(0);
	m_meter->setTextVisible(false); m_meter->setFixedHeight(4);
	m_meter->setStyleSheet(
		"QProgressBar{background:rgba(30,30,45,220);border:none;border-radius:2px;}"
		"QProgressBar::chunk{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
		"stop:0 #4a8af4,stop:0.6 #64dca0,stop:1 #7cf);border-radius:2px;}");
	outer->addWidget(m_meter);
	// Overlaid graph — MIDI In (blue, behind) + Ctrl Out (cyan, front)
	m_graph = new MiniGraph(QColor(100,220,180), 120, min, max, this);
	m_graph->set_secondary_color(QColor(80,140,220));
	m_graph->set_title(name);
	m_graph->setFixedHeight(60);
	outer->addWidget(m_graph);
	setStyleSheet("background:rgba(30,30,48,220);border-radius:6px;");
}
void MasterPreview::set_value(double val) {
	m_value_label->setText(QString::number(val,'f',3));
	double norm = (m_max==m_min) ? 0 : qBound(0.0,(val-m_min)/(m_max-m_min),1.0);
	m_meter->setValue(int(norm*1000));
	// Push dual: primary = output, secondary = raw input (both in output range)
	m_graph->push_dual(val, m_last_raw_norm);
	m_output_dot->pulse();
}
void MasterPreview::set_static_value(double val) {
	m_value_label->setText(QString::number(val,'f',3));
	double norm = (m_max==m_min) ? 0 : qBound(0.0,(val-m_min)/(m_max-m_min),1.0);
	m_meter->setValue(int(norm*1000));
	// No graph push, no dot pulse
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

// ===== BindingPanel =======================================================
BindingPanel::BindingPanel(int index, int map_mode,
	double out_min, double out_max, const QStringList &combo_items, QWidget *parent)
	: QFrame(parent), m_index(index), m_map_mode(map_mode)
	, m_default_out_min(out_min), m_default_out_max(out_max), m_combo_items(combo_items)
{ setFrameShape(QFrame::StyledPanel); setup_ui(); }

void BindingPanel::setup_ui() {
	auto *top = new QVBoxLayout(this); top->setContentsMargins(4,4,4,4); top->setSpacing(2);
	// Header
	auto *hdr = new QHBoxLayout();
	m_header_dot = new ActivityDot(QColor(80,180,255), this);
	m_header_btn = new QPushButton(QString("▶ Binding #%1").arg(m_index+1),this);
	m_header_btn->setFlat(true); m_header_btn->setStyleSheet("text-align:left;font-weight:bold;padding:4px;");
	m_header_enabled = new QCheckBox(this); m_header_enabled->setChecked(true);
	m_header_remove = new QPushButton("✕",this); m_header_remove->setFixedSize(20,20); m_header_remove->setStyleSheet("color:#e74c3c;");
	hdr->addWidget(m_header_dot); hdr->addWidget(m_header_btn,1); hdr->addWidget(m_header_enabled); hdr->addWidget(m_header_remove);
	top->addLayout(hdr);

	m_body = new QWidget(this); m_body->setVisible(false);
	auto *bl = new QVBoxLayout(m_body); bl->setContentsMargins(8,4,8,4); bl->setSpacing(4);

	// 1. MIDI Source
	auto *src = new QGroupBox("MIDI Source", m_body);
	auto *sf = new QFormLayout(src); sf->setContentsMargins(8,4,8,4); sf->setSpacing(3);
	m_device_combo = new QComboBox(src); sf->addRow("Device:",m_device_combo);
	m_channel_spin = new QSpinBox(src); m_channel_spin->setRange(0,15); sf->addRow("Channel:",m_channel_spin);
	m_cc_spin = new QSpinBox(src); m_cc_spin->setRange(0,127); sf->addRow("CC/Note:",m_cc_spin);
	bl->addWidget(src);

	// 2. Pre-Filters (raw domain)
	{
		m_pre_filter_group = new QGroupBox("Pre-Filters (Raw MIDI)", m_body);
		auto *pfv = new QVBoxLayout(m_pre_filter_group); pfv->setContentsMargins(4,4,4,4); pfv->setSpacing(2);
		m_pre_filter_layout = new QVBoxLayout(); m_pre_filter_layout->setSpacing(2);
		pfv->addLayout(m_pre_filter_layout);
		auto *pfa = new QPushButton("+ Add Pre-Filter",m_pre_filter_group);
		pfa->setStyleSheet("color:#2ecc71;font-size:10px;");
		pfv->addWidget(pfa);
		bl->addWidget(m_pre_filter_group);
		connect(pfa,&QPushButton::clicked,this,[this]{ add_pre_filter({}); emit changed(); });
	}

	// 3. Value Mapping (Range mode)
	if (m_map_mode == MidiPortBinding::Range) {
		m_range_group = new QGroupBox("Mapping (Input→Output)", m_body);
		auto *rf = new QFormLayout(m_range_group); rf->setContentsMargins(8,4,8,4); rf->setSpacing(3);
		m_input_min_spin = new QSpinBox(m_range_group); m_input_min_spin->setRange(0,127); rf->addRow("In Min:",m_input_min_spin);
		m_input_max_spin = new QSpinBox(m_range_group); m_input_max_spin->setRange(0,127); m_input_max_spin->setValue(127); rf->addRow("In Max:",m_input_max_spin);
		m_output_min_spin = new QDoubleSpinBox(m_range_group); m_output_min_spin->setRange(-9999,9999); m_output_min_spin->setDecimals(2); m_output_min_spin->setValue(m_default_out_min); rf->addRow("Out Min:",m_output_min_spin);
		m_output_max_spin = new QDoubleSpinBox(m_range_group); m_output_max_spin->setRange(-9999,9999); m_output_max_spin->setDecimals(2); m_output_max_spin->setValue(m_default_out_max); rf->addRow("Out Max:",m_output_max_spin);
		bl->addWidget(m_range_group);
	}

	// 4. Interpolation Chain (Range or Select mode)
	if (m_map_mode == MidiPortBinding::Range || m_map_mode == MidiPortBinding::Select) {
		m_interp_group = new QGroupBox("Interpolation Chain", m_body);
		auto *iv = new QVBoxLayout(m_interp_group); iv->setContentsMargins(4,4,4,4); iv->setSpacing(2);
		m_interp_layout = new QVBoxLayout(); m_interp_layout->setSpacing(2);
		iv->addLayout(m_interp_layout);
		auto *ia = new QPushButton("+ Add Interpolation",m_interp_group);
		ia->setStyleSheet("color:#3498db;font-size:10px;");
		iv->addWidget(ia);
		bl->addWidget(m_interp_group);
		connect(ia,&QPushButton::clicked,this,[this]{ add_interp_stage({}); emit changed(); });
	}

	// Threshold group (Toggle/Trigger)
	if (m_map_mode == MidiPortBinding::Toggle || m_map_mode == MidiPortBinding::Trigger) {
		m_threshold_group = new QGroupBox("Threshold", m_body);
		auto *tf = new QFormLayout(m_threshold_group); tf->setContentsMargins(8,4,8,4); tf->setSpacing(3);
		m_threshold_spin = new QSpinBox(m_threshold_group); m_threshold_spin->setRange(0,127); m_threshold_spin->setValue(63);
		tf->addRow("Value:",m_threshold_spin);
		if (m_map_mode == MidiPortBinding::Toggle) {
			m_toggle_mode_combo = new QComboBox(m_threshold_group);
			m_toggle_mode_combo->addItem("Toggle", 0);
			m_toggle_mode_combo->addItem("Check (Set On)", 1);
			m_toggle_mode_combo->addItem("Uncheck (Set Off)", 2);
			tf->addRow("Mode:", m_toggle_mode_combo);
			connect(m_toggle_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ emit changed(); });
		}
		if (m_map_mode == MidiPortBinding::Trigger) {
			m_continuous_check = new QCheckBox("Continuous fire",m_threshold_group); tf->addRow("",m_continuous_check);
			m_continuous_interval_spin = new QSpinBox(m_threshold_group);
			m_continuous_interval_spin->setRange(16,5000); m_continuous_interval_spin->setValue(100); m_continuous_interval_spin->setSuffix(" ms");
			tf->addRow("Interval:",m_continuous_interval_spin);
		}
		bl->addWidget(m_threshold_group);
	}

	// 5. Post-Filters (output domain)
	{
		m_post_filter_group = new QGroupBox("Post-Filters (Output)", m_body);
		auto *pov = new QVBoxLayout(m_post_filter_group); pov->setContentsMargins(4,4,4,4); pov->setSpacing(2);
		m_post_filter_layout = new QVBoxLayout(); m_post_filter_layout->setSpacing(2);
		pov->addLayout(m_post_filter_layout);
		auto *poa = new QPushButton("+ Add Post-Filter",m_post_filter_group);
		poa->setStyleSheet("color:#e67e22;font-size:10px;");
		pov->addWidget(poa);
		bl->addWidget(m_post_filter_group);
		connect(poa,&QPushButton::clicked,this,[this]{ add_post_filter({}); emit changed(); });
	}

	// 6. Action — not for Select
	if (m_map_mode != MidiPortBinding::Select) {
		m_action_group = new QGroupBox("Action", m_body);
		auto *af = new QFormLayout(m_action_group); af->setContentsMargins(8,4,8,4); af->setSpacing(3);
		m_action_combo = new QComboBox(m_action_group);
		m_action_combo->addItem("Set Value",0); m_action_combo->addItem("Trigger",1);
		af->addRow("Mode:",m_action_combo);
		connect(m_action_combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{emit changed();});
		bl->addWidget(m_action_group);
	}

	// 7. Options
	m_invert_check = new QCheckBox("Invert", m_body);
	bl->addWidget(m_invert_check);
	top->addWidget(m_body);

	// Signals — header
	connect(m_header_btn,&QPushButton::clicked,this,[this]{emit expand_requested(m_index);});
	connect(m_header_remove,&QPushButton::clicked,this,[this]{emit remove_requested(m_index);});
	connect(m_header_enabled,&QCheckBox::toggled,this,[this]{emit changed();});
	connect(m_invert_check,&QCheckBox::toggled,this,[this]{emit changed();});
	// Signals — MIDI source
	connect(m_device_combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{emit changed();});
	connect(m_channel_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	connect(m_cc_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	// Signals — Range mapping
	if (m_input_min_spin)
		connect(m_input_min_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_input_max_spin)
		connect(m_input_max_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_output_min_spin)
		connect(m_output_min_spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_output_max_spin)
		connect(m_output_max_spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{emit changed();});
	// Signals — Threshold/Trigger extras
	if (m_threshold_spin)
		connect(m_threshold_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
	if (m_continuous_check)
		connect(m_continuous_check,&QCheckBox::toggled,this,[this]{emit changed();});
	if (m_continuous_interval_spin)
		connect(m_continuous_interval_spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this]{emit changed();});
}

void BindingPanel::pulse_header_activity() {
	if (m_header_dot) m_header_dot->pulse();
}

void BindingPanel::add_pre_filter(const FilterStage &s) {
	int idx = m_pre_filter_rows.size();
	auto *row = new FilterStageRow(idx, QColor(46,204,113), m_pre_filter_group);
	row->set_title_prefix("Pre-Filter");
	row->m_graph->set_range(0.0, 127.0); // Pre-filters operate on raw MIDI domain
	if (s.type||s.param1||s.param2||!s.enabled) row->load(s);
	row->update_title("Pre-Filter", idx+1);
	m_pre_filter_layout->addWidget(row); m_pre_filter_rows.append(row);
	connect(row,&StageRow::changed,this,[this]{emit changed();});
	connect(row,&StageRow::move_up,this,[this](int i){ if(i>0){std::swap(m_pre_filter_rows[i],m_pre_filter_rows[i-1]); rebuild_indices(m_pre_filter_rows,m_pre_filter_layout); emit changed();}});
	connect(row,&StageRow::move_down,this,[this](int i){ if(i<m_pre_filter_rows.size()-1){std::swap(m_pre_filter_rows[i],m_pre_filter_rows[i+1]); rebuild_indices(m_pre_filter_rows,m_pre_filter_layout); emit changed();}});
	connect(row,&StageRow::remove,this,[this](int i){ if(i>=0&&i<m_pre_filter_rows.size()){auto *w=m_pre_filter_rows.takeAt(i); m_pre_filter_layout->removeWidget(w); w->deleteLater(); rebuild_indices(m_pre_filter_rows,m_pre_filter_layout); emit changed();}});
}
void BindingPanel::add_interp_stage(const InterpStage &s) {
	int idx = m_interp_rows.size();
	auto *row = new InterpStageRow(idx, m_interp_group);
	row->set_title_prefix("Interp");
	if (s.type||s.param1||s.param2||!s.enabled) row->load(s);
	row->update_title("Interp", idx+1);
	m_interp_layout->addWidget(row); m_interp_rows.append(row);
	connect(row,&StageRow::changed,this,[this]{emit changed();});
	connect(row,&StageRow::move_up,this,[this](int i){ if(i>0){std::swap(m_interp_rows[i],m_interp_rows[i-1]); rebuild_indices(m_interp_rows,m_interp_layout); emit changed();}});
	connect(row,&StageRow::move_down,this,[this](int i){ if(i<m_interp_rows.size()-1){std::swap(m_interp_rows[i],m_interp_rows[i+1]); rebuild_indices(m_interp_rows,m_interp_layout); emit changed();}});
	connect(row,&StageRow::remove,this,[this](int i){ if(i>=0&&i<m_interp_rows.size()){auto *w=m_interp_rows.takeAt(i); m_interp_layout->removeWidget(w); w->deleteLater(); rebuild_indices(m_interp_rows,m_interp_layout); emit changed();}});
}
void BindingPanel::add_post_filter(const FilterStage &s) {
	int idx = m_post_filter_rows.size();
	auto *row = new FilterStageRow(idx, QColor(230,126,34), m_post_filter_group);
	row->set_title_prefix("Post-Filter");
	// Post-filters operate on output domain
	double omin = m_output_min_spin ? m_output_min_spin->value() : 0.0;
	double omax = m_output_max_spin ? m_output_max_spin->value() : 1.0;
	row->m_graph->set_range(omin, omax);
	if (s.type||s.param1||s.param2||!s.enabled) row->load(s);
	row->update_title("Post-Filter", idx+1);
	m_post_filter_layout->addWidget(row); m_post_filter_rows.append(row);
	connect(row,&StageRow::changed,this,[this]{emit changed();});
	connect(row,&StageRow::move_up,this,[this](int i){ if(i>0){std::swap(m_post_filter_rows[i],m_post_filter_rows[i-1]); rebuild_indices(m_post_filter_rows,m_post_filter_layout); emit changed();}});
	connect(row,&StageRow::move_down,this,[this](int i){ if(i<m_post_filter_rows.size()-1){std::swap(m_post_filter_rows[i],m_post_filter_rows[i+1]); rebuild_indices(m_post_filter_rows,m_post_filter_layout); emit changed();}});
	connect(row,&StageRow::remove,this,[this](int i){ if(i>=0&&i<m_post_filter_rows.size()){auto *w=m_post_filter_rows.takeAt(i); m_post_filter_layout->removeWidget(w); w->deleteLater(); rebuild_indices(m_post_filter_rows,m_post_filter_layout); emit changed();}});
}

void BindingPanel::rebuild_indices(QVector<StageRow*> &rows, QVBoxLayout *layout) {
	for (int i=0;i<rows.size();i++) {
		layout->removeWidget(rows[i]);
		rows[i]->set_index(i);
		rows[i]->update_title({}, i+1);
	}
	for (auto *r : rows) layout->addWidget(r);
}

void BindingPanel::load_from_binding(const MidiPortBinding &b) {
	m_header_enabled->setChecked(b.enabled);
	// device_index -1 means "any" → combo index 0; otherwise offset by +1 for "(Any)" entry
	int combo_idx = (b.device_index < 0) ? 0 : b.device_index + 1;
	if(combo_idx < m_device_combo->count()) m_device_combo->setCurrentIndex(combo_idx);
	m_channel_spin->setValue(b.channel); m_cc_spin->setValue(b.data1);
	if(m_input_min_spin) m_input_min_spin->setValue(b.input_min);
	if(m_input_max_spin) m_input_max_spin->setValue(b.input_max);
	if(m_output_min_spin) m_output_min_spin->setValue(b.output_min);
	if(m_output_max_spin) m_output_max_spin->setValue(b.output_max);
	if(m_threshold_spin) m_threshold_spin->setValue(b.threshold);
	if(m_toggle_mode_combo) { int ti=m_toggle_mode_combo->findData(b.toggle_mode); if(ti>=0) m_toggle_mode_combo->setCurrentIndex(ti); }
	if(m_continuous_check) m_continuous_check->setChecked(b.continuous_fire);
	if(m_continuous_interval_spin) m_continuous_interval_spin->setValue(b.continuous_fire_interval_ms);
	m_invert_check->setChecked(b.invert);
	m_is_encoder=b.is_encoder; m_encoder_mode=b.encoder_mode; m_encoder_sensitivity=b.encoder_sensitivity;
	if(m_action_combo) { int ai=m_action_combo->findData(static_cast<int>(b.action_mode)); if(ai>=0)m_action_combo->setCurrentIndex(ai); }
	if(m_action_p1) m_action_p1->setValue(b.action_param1);
	if(m_action_p2) m_action_p2->setValue(b.action_param2);
	for(const auto &f : b.pre_filters) add_pre_filter(f);
	for(const auto &s : b.interp_stages) add_interp_stage(s);
	for(const auto &f : b.post_filters) add_post_filter(f);
	update_header();
}

MidiPortBinding BindingPanel::build_binding(const QString &port_id) const {
	MidiPortBinding b;
	b.port_id=port_id; b.enabled=m_header_enabled->isChecked();
	// combo index 0 = "(Any)" → device_index -1; otherwise offset by -1
	b.device_index = m_device_combo->currentIndex() - 1;
	b.channel=m_channel_spin->value(); b.data1=m_cc_spin->value();
	b.map_mode=static_cast<MidiPortBinding::MapMode>(m_map_mode);
	if(m_input_min_spin) b.input_min=m_input_min_spin->value();
	if(m_input_max_spin) b.input_max=m_input_max_spin->value();
	if(m_output_min_spin) b.output_min=m_output_min_spin->value();
	if(m_output_max_spin) b.output_max=m_output_max_spin->value();
	if(m_threshold_spin) b.threshold=m_threshold_spin->value();
	if(m_toggle_mode_combo) b.toggle_mode=m_toggle_mode_combo->currentData().toInt();
	if(m_continuous_check) b.continuous_fire=m_continuous_check->isChecked();
	if(m_continuous_interval_spin) b.continuous_fire_interval_ms=m_continuous_interval_spin->value();
	b.invert=m_invert_check->isChecked();
	b.is_encoder=m_is_encoder; b.encoder_mode=m_encoder_mode; b.encoder_sensitivity=m_encoder_sensitivity;
	if(m_action_combo) b.action_mode=static_cast<ActionMode>(m_action_combo->currentData().toInt());
	if(m_action_p1) b.action_param1=m_action_p1->value();
	if(m_action_p2) b.action_param2=m_action_p2->value();
	for(auto *r : m_pre_filter_rows) b.pre_filters.append(static_cast<FilterStageRow*>(r)->build());
	for(auto *r : m_interp_rows) b.interp_stages.append(static_cast<InterpStageRow*>(r)->build());
	for(auto *r : m_post_filter_rows) b.post_filters.append(static_cast<FilterStageRow*>(r)->build());
	return b;
}

void BindingPanel::reset_to_defaults() {
	m_header_enabled->setChecked(true);
	if(m_input_min_spin) m_input_min_spin->setValue(0);
	if(m_input_max_spin) m_input_max_spin->setValue(127);
	if(m_output_min_spin) m_output_min_spin->setValue(m_default_out_min);
	if(m_output_max_spin) m_output_max_spin->setValue(m_default_out_max);
	m_invert_check->setChecked(false);
	if(m_action_combo) m_action_combo->setCurrentIndex(0);
	qDeleteAll(m_pre_filter_rows); m_pre_filter_rows.clear();
	qDeleteAll(m_interp_rows); m_interp_rows.clear();
	qDeleteAll(m_post_filter_rows); m_post_filter_rows.clear();
	// Default: add one Linear interp so mapping works out of the box
	if (m_map_mode == MidiPortBinding::Range) add_interp_stage({});
}

void BindingPanel::populate_devices(const QStringList &d) { m_device_combo->clear(); m_device_combo->addItems(d); }
void BindingPanel::set_learned_source(int dev, int ch, int cc, bool enc, EncoderMode em, double es) {
	// dev is raw device index; combo has "(Any)" at 0, so offset +1
	int combo_idx = (dev < 0) ? 0 : dev + 1;
	if(combo_idx < m_device_combo->count()) m_device_combo->setCurrentIndex(combo_idx);
	m_channel_spin->setValue(ch); m_cc_spin->setValue(cc);
	m_is_encoder=enc; m_encoder_mode=em; m_encoder_sensitivity=es;
	update_header(); emit changed();
}
void BindingPanel::set_expanded(bool e) { m_expanded=e; m_body->setVisible(e); update_header(); }
bool BindingPanel::is_expanded() const { return m_expanded; }
void BindingPanel::set_index(int i) { m_index=i; update_header(); }
int BindingPanel::index() const { return m_index; }
void BindingPanel::update_header() {
	m_header_btn->setText(QString("%1 Binding #%2  [Ch%3 CC%4]")
		.arg(m_expanded?"▼":"▶").arg(m_index+1).arg(m_channel_spin->value()).arg(m_cc_spin->value()));
}

// Sync UI parameters into m_preview_state, preserving runtime state for matching stages
void BindingPanel::sync_preview_params() {
	// Sync simple binding params from UI
	m_preview_state.map_mode = static_cast<MidiPortBinding::MapMode>(m_map_mode);
	m_preview_state.device_index = m_device_combo ? m_device_combo->currentIndex() - 1 : -1;
	m_preview_state.channel = m_channel_spin ? m_channel_spin->value() : 0;
	m_preview_state.data1 = m_cc_spin ? m_cc_spin->value() : 0;
	if (m_input_min_spin) m_preview_state.input_min = m_input_min_spin->value();
	if (m_input_max_spin) m_preview_state.input_max = m_input_max_spin->value();
	if (m_output_min_spin) m_preview_state.output_min = m_output_min_spin->value();
	if (m_output_max_spin) m_preview_state.output_max = m_output_max_spin->value();
	if (m_threshold_spin) m_preview_state.threshold = m_threshold_spin->value();
	m_preview_state.invert = m_invert_check ? m_invert_check->isChecked() : false;
	if (m_action_combo) m_preview_state.action_mode = static_cast<ActionMode>(m_action_combo->currentData().toInt());
	if (m_action_p1) m_preview_state.action_param1 = m_action_p1->value();
	if (m_action_p2) m_preview_state.action_param2 = m_action_p2->value();

	// Sync pre-filters: preserve runtime state when type matches
	auto sync_filters = [](const QVector<StageRow*> &rows, QVector<FilterStage> &stages) {
		int new_size = rows.size();
		// Grow/shrink
		while (stages.size() > new_size) stages.removeLast();
		while (stages.size() < new_size) stages.append(FilterStage{});
		for (int i = 0; i < new_size; i++) {
			auto built = static_cast<FilterStageRow*>(rows[i])->build();
			if (stages[i].type != built.type) {
				// Type changed — full reset
				stages[i] = built;
			} else {
				// Same type — update params only, keep runtime
				stages[i].enabled = built.enabled;
				stages[i].param1 = built.param1;
				stages[i].param2 = built.param2;
			}
		}
	};
	sync_filters(m_pre_filter_rows, m_preview_state.pre_filters);
	sync_filters(m_post_filter_rows, m_preview_state.post_filters);

	// Sync interp stages
	{
		int new_size = m_interp_rows.size();
		while (m_preview_state.interp_stages.size() > new_size)
			m_preview_state.interp_stages.removeLast();
		while (m_preview_state.interp_stages.size() < new_size)
			m_preview_state.interp_stages.append(InterpStage{});
		for (int i = 0; i < new_size; i++) {
			auto built = static_cast<InterpStageRow*>(m_interp_rows[i])->build();
			if (m_preview_state.interp_stages[i].type != built.type) {
				m_preview_state.interp_stages[i] = built;
			} else {
				m_preview_state.interp_stages[i].enabled = built.enabled;
				m_preview_state.interp_stages[i].param1 = built.param1;
				m_preview_state.interp_stages[i].param2 = built.param2;
			}
		}
	}
}

double BindingPanel::update_pipeline_preview(int raw) {
	sync_preview_params();
	auto p = m_preview_state.preview_pipeline(raw);
	m_last_preview = p;
	// Pulse + preview pre-filters
	for(int i=0;i<m_pre_filter_rows.size()&&i<p.after_pre_filter.size();i++) {
		double in = (i==0) ? double(raw) : p.after_pre_filter[i-1];
		m_pre_filter_rows[i]->set_preview(in, p.after_pre_filter[i]);
		m_pre_filter_rows[i]->pulse_activity();
	}
	// Pulse + preview interps
	for(int i=0;i<m_interp_rows.size()&&i<p.after_interp.size();i++) {
		double in = (i==0) ? p.normalized : p.after_interp[i-1];
		m_interp_rows[i]->set_preview(in, p.after_interp[i]);
		m_interp_rows[i]->pulse_activity();
	}
	// Pulse + preview post-filters
	for(int i=0;i<m_post_filter_rows.size()&&i<p.after_post_filter.size();i++) {
		double in = (i==0) ? p.mapped : p.after_post_filter[i-1];
		m_post_filter_rows[i]->set_preview(in, p.after_post_filter[i]);
		m_post_filter_rows[i]->pulse_activity();
	}
	return p.final_value;
}

bool BindingPanel::needs_preview_convergence() const {
	return m_preview_state.needs_convergence();
}

double BindingPanel::sync_pipeline_state(int raw) {
	sync_preview_params();
	auto p = m_preview_state.preview_pipeline(raw);
	m_last_preview = p;
	// Labels only — no graph push, no dot pulse
	for(int i=0;i<m_pre_filter_rows.size()&&i<p.after_pre_filter.size();i++) {
		double in = (i==0) ? double(raw) : p.after_pre_filter[i-1];
		m_pre_filter_rows[i]->set_preview_label(in, p.after_pre_filter[i]);
	}
	for(int i=0;i<m_interp_rows.size()&&i<p.after_interp.size();i++) {
		double in = (i==0) ? p.normalized : p.after_interp[i-1];
		m_interp_rows[i]->set_preview_label(in, p.after_interp[i]);
	}
	for(int i=0;i<m_post_filter_rows.size()&&i<p.after_post_filter.size();i++) {
		double in = (i==0) ? p.mapped : p.after_post_filter[i-1];
		m_post_filter_rows[i]->set_preview_label(in, p.after_post_filter[i]);
	}
	return p.final_value;
}

// ===== ControlAssignPopup =================================================
static const char *POPUP_STYLE =
	"QDialog{background:rgba(28,28,36,245);}"
	"QGroupBox{font-size:11px;font-weight:bold;color:#aab;border:1px solid rgba(255,255,255,0.08);border-radius:4px;margin-top:8px;padding-top:10px;}"
	"QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:#8af;}"
	"QLabel{color:#ccc;font-size:11px;}"
	"QSpinBox,QDoubleSpinBox,QComboBox{background:rgba(40,40,55,200);color:#ddd;border:1px solid rgba(255,255,255,0.1);border-radius:3px;padding:2px 4px;font-size:11px;}"
	"QCheckBox{color:#bbb;font-size:11px;}"
	"QPushButton{background:rgba(50,60,80,200);color:#ccc;border:1px solid rgba(255,255,255,0.1);border-radius:4px;padding:4px 10px;font-size:11px;}"
	"QPushButton:hover{background:rgba(60,80,120,220);color:#fff;}"
	"QPushButton:disabled{color:#666;background:rgba(40,40,50,150);}"
	"QTabWidget::pane{border:1px solid rgba(255,255,255,0.08);border-radius:4px;}"
	"QTabBar::tab{background:rgba(40,40,55,200);color:#999;padding:4px 12px;border-top-left-radius:4px;border-top-right-radius:4px;}"
	"QTabBar::tab:selected{background:rgba(60,70,100,220);color:#fff;}";

ControlAssignPopup::ControlAssignPopup(const QString &port_id,
	const QString &display_name, int map_mode,
	double output_min, double output_max,
	const QStringList &combo_items, MidiAdapter *adapter, QWidget *parent)
	: QDialog(parent, Qt::Dialog | Qt::WindowCloseButtonHint)
	, m_port_id(port_id), m_display_name(display_name), m_map_mode(map_mode)
	, m_default_out_min(output_min), m_default_out_max(output_max)
	, m_combo_items(combo_items), m_adapter(adapter)
{
	setWindowTitle(QString("MIDI Assign — %1").arg(display_name));
	setAttribute(Qt::WA_DeleteOnClose);
	setMinimumSize(540, 400);
	resize(580, 550);
	setStyleSheet(POPUP_STYLE);
	setup_ui();
	populate_devices();
	// Hot-detect MIDI device plug/unplug
	if (m_adapter && m_adapter->backend()) {
		auto *be = m_adapter->backend();
		connect(be, &MidiBackend::devices_changed, this, [this]{
			populate_devices();
			// Flash title to show device change detected
			QString orig = windowTitle();
			setWindowTitle(orig + "  🔌");
			QTimer::singleShot(1500, this, [this, orig]{ setWindowTitle(orig); });
		});
		be->start_device_poll(2000);
	}
	sync_panels_from_adapter();
	sync_outputs_from_adapter();
	// Preview convergence timer — keeps graphs updating during time-based filters
	m_preview_timer = new QTimer(this);
	m_preview_timer->setInterval(16);
	connect(m_preview_timer, &QTimer::timeout, this, &ControlAssignPopup::on_preview_tick);
	m_preview_timer->start();
	mark_clean();
	if (m_adapter && m_adapter->backend())
		connect(m_adapter->backend(), &MidiBackend::midi_message, this, &ControlAssignPopup::on_raw_midi);
	// Initial state so labels/meters show current values (no graph push)
	sync_ui_state();
}
ControlAssignPopup::~ControlAssignPopup() { emit closed(); }

void ControlAssignPopup::setup_ui() {
	auto *root = new QVBoxLayout(this); root->setContentsMargins(10,10,10,10); root->setSpacing(6);
	// Master Preview
	m_master_preview = new MasterPreview(m_display_name, m_default_out_min, m_default_out_max, this);
	root->addWidget(m_master_preview);
	// Pipeline visual button — placed in master preview header
	m_pipeline_btn = new QPushButton(QString::fromUtf8("\xF0\x9F\x93\x8A"), this);
	m_pipeline_btn->setFixedSize(26, 22);
	m_pipeline_btn->setToolTip("Pipeline View");
	m_pipeline_btn->setStyleSheet("QPushButton{font-size:12px;padding:0;border:1px solid rgba(100,180,255,60);border-radius:3px;background:rgba(40,40,60,180);}"
		"QPushButton:hover{background:rgba(60,60,90,220);}");
	connect(m_pipeline_btn, &QPushButton::clicked, this, [this]{
		if (m_pipeline_visual) { m_pipeline_visual->raise(); m_pipeline_visual->activateWindow(); return; }
		m_pipeline_visual = new PipelineVisualDialog(m_display_name, m_default_out_min, m_default_out_max, window());
		m_pipeline_visual->show();
		// Set static state — no time-domain push
		sync_ui_state();
	});
	m_master_preview->add_pipeline_button(m_pipeline_btn);
	// Status
	m_status_label = new QLabel("Ready",this); m_status_label->setStyleSheet("color:#888;font-size:10px;font-style:italic;");
	root->addWidget(m_status_label);
	// Tabs
	m_tab_widget = new QTabWidget(this); root->addWidget(m_tab_widget,1);

	// === Input tab ===
	auto *in_tab = new QWidget(); auto *il = new QVBoxLayout(in_tab); il->setContentsMargins(4,4,4,4); il->setSpacing(4);
	m_scroll_area = new QScrollArea(in_tab); m_scroll_area->setWidgetResizable(true); m_scroll_area->setFrameShape(QFrame::NoFrame);
	m_panel_container = new QWidget(); m_panel_layout = new QVBoxLayout(m_panel_container);
	m_panel_layout->setContentsMargins(0,0,0,0); m_panel_layout->setSpacing(4); m_panel_layout->addStretch();
	m_scroll_area->setWidget(m_panel_container); il->addWidget(m_scroll_area,1);
	auto *ib = new QHBoxLayout();
	m_add_btn = new QPushButton("+ Add Binding",in_tab);
	m_learn_btn = new QPushButton("🎹 Learn",in_tab); m_learn_btn->setStyleSheet("QPushButton{background:rgba(46,204,113,180);color:#fff;}");
	ib->addWidget(m_add_btn); ib->addWidget(m_learn_btn); ib->addStretch();
	il->addLayout(ib);
	m_tab_widget->addTab(in_tab,"Input");

	// === Output tab ===
	auto *ot = new QWidget(); auto *ol = new QVBoxLayout(ot); ol->setContentsMargins(4,4,4,4); ol->setSpacing(4);
	m_output_scroll = new QScrollArea(ot); m_output_scroll->setWidgetResizable(true); m_output_scroll->setFrameShape(QFrame::NoFrame);
	m_output_container = new QWidget(); m_output_layout = new QVBoxLayout(m_output_container);
	m_output_layout->setContentsMargins(0,0,0,0); m_output_layout->setSpacing(4); m_output_layout->addStretch();
	m_output_scroll->setWidget(m_output_container); ol->addWidget(m_output_scroll,1);
	m_add_output_btn = new QPushButton("+ Add Output",ot); ol->addWidget(m_add_output_btn);
	m_tab_widget->addTab(ot,"Output");

	// Apply
	m_apply_btn = new QPushButton("Apply",this);
	m_apply_btn->setStyleSheet("QPushButton{background:rgba(52,152,219,200);color:#fff;font-weight:bold;padding:6px 16px;}QPushButton:disabled{background:rgba(40,40,50,150);color:#666;}");
	m_apply_btn->setEnabled(false);
	root->addWidget(m_apply_btn);

	// Monitor
	m_monitor_toggle = new QPushButton("MIDI Monitor ▶",this); m_monitor_toggle->setFlat(true); m_monitor_toggle->setStyleSheet("color:#888;font-size:10px;");
	root->addWidget(m_monitor_toggle);
	m_monitor_container = new QWidget(this); m_monitor_container->setVisible(false);
	auto *ml = new QVBoxLayout(m_monitor_container); ml->setContentsMargins(0,0,0,0);
	m_monitor_log = new QPlainTextEdit(m_monitor_container); m_monitor_log->setReadOnly(true); m_monitor_log->setMaximumHeight(80);
	m_monitor_log->setStyleSheet("background:rgba(20,20,28,200);color:#8f8;font-family:monospace;font-size:10px;border:1px solid rgba(255,255,255,0.05);border-radius:3px;");
	auto *clr = new QPushButton("Clear",m_monitor_container); clr->setFixedWidth(50);
	auto *mr = new QHBoxLayout(); mr->addWidget(m_monitor_log,1); mr->addWidget(clr,0,Qt::AlignTop);
	ml->addLayout(mr); root->addWidget(m_monitor_container);

	// Connect
	connect(m_add_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_add_clicked);
	connect(m_learn_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_learn_clicked);
	connect(m_apply_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_apply_clicked);
	connect(m_add_output_btn,&QPushButton::clicked,this,&ControlAssignPopup::on_add_output_clicked);
	connect(m_monitor_toggle,&QPushButton::clicked,this,[this]{toggle_monitor(!m_monitor_container->isVisible());});
	connect(clr,&QPushButton::clicked,this,[this]{m_monitor_log->clear();m_monitor_msg_count=0;});
	if(m_adapter){
		connect(m_adapter,&MidiAdapter::binding_learned,this,&ControlAssignPopup::on_binding_learned);
		connect(m_adapter,&MidiAdapter::learn_cancelled,this,&ControlAssignPopup::on_learn_cancelled);
	}
}

void ControlAssignPopup::populate_devices() {
	m_cached_in_devices.clear(); m_cached_in_devices<<"(Any)";
	m_cached_out_devices.clear(); m_cached_out_devices<<"(Any)";
	if(m_adapter&&m_adapter->backend()){
		auto *be=m_adapter->backend();
		for(auto &d:be->available_input_devices()) m_cached_in_devices<<d;
		for(auto &d:be->available_output_devices()) m_cached_out_devices<<d;
	}
	for(auto *p:m_panels) p->populate_devices(m_cached_in_devices);
	for(auto *p:m_output_panels) p->populate_devices(m_cached_out_devices);
}

void ControlAssignPopup::sync_panels_from_adapter() {
	if(!m_adapter) return;
	for(const auto &b : m_adapter->bindings_for(m_port_id)) {
		int idx=m_panels.size();
		auto *p=new BindingPanel(idx,m_map_mode,m_default_out_min,m_default_out_max,m_combo_items,m_panel_container);
		p->populate_devices(m_cached_in_devices); p->load_from_binding(b);
		m_panel_layout->insertWidget(m_panel_layout->count()-1,p); m_panels.append(p);
		connect(p,&BindingPanel::expand_requested,this,&ControlAssignPopup::on_panel_expand);
		connect(p,&BindingPanel::remove_requested,this,&ControlAssignPopup::on_panel_remove);
		connect(p,&BindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
		connect(p,&BindingPanel::changed,this,&ControlAssignPopup::sync_ui_state);
	}
	if(!m_panels.isEmpty()){m_panels.first()->set_expanded(true);m_active_panel=0;}
}

void ControlAssignPopup::sync_outputs_from_adapter() {
	if(!m_adapter) return;
	for(const auto &o : m_adapter->outputs_for(m_port_id)) {
		int idx=m_output_panels.size();
		auto *p=new OutputBindingPanel(idx,m_output_container);
		p->populate_devices(m_cached_out_devices); p->load(o);
		m_output_layout->insertWidget(m_output_layout->count()-1,p); m_output_panels.append(p);
		connect(p,&OutputBindingPanel::expand_requested,this,&ControlAssignPopup::on_output_expand);
		connect(p,&OutputBindingPanel::remove_requested,this,&ControlAssignPopup::on_output_remove);
		connect(p,&OutputBindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
	}
}

void ControlAssignPopup::mark_dirty(){m_dirty=true;m_apply_btn->setEnabled(true);}
void ControlAssignPopup::mark_clean(){m_dirty=false;m_apply_btn->setEnabled(false);}

void ControlAssignPopup::on_add_clicked() {
	int idx=m_panels.size();
	auto *p=new BindingPanel(idx,m_map_mode,m_default_out_min,m_default_out_max,m_combo_items,m_panel_container);
	p->populate_devices(m_cached_in_devices); p->reset_to_defaults();
	m_panel_layout->insertWidget(m_panel_layout->count()-1,p); m_panels.append(p);
	connect(p,&BindingPanel::expand_requested,this,&ControlAssignPopup::on_panel_expand);
	connect(p,&BindingPanel::remove_requested,this,&ControlAssignPopup::on_panel_remove);
	connect(p,&BindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
	connect(p,&BindingPanel::changed,this,&ControlAssignPopup::sync_ui_state);
	on_panel_expand(idx); mark_dirty();
}
void ControlAssignPopup::on_add_output_clicked() {
	int idx=m_output_panels.size();
	auto *p=new OutputBindingPanel(idx,m_output_container);
	p->populate_devices(m_cached_out_devices);
	m_output_layout->insertWidget(m_output_layout->count()-1,p); m_output_panels.append(p);
	connect(p,&OutputBindingPanel::expand_requested,this,&ControlAssignPopup::on_output_expand);
	connect(p,&OutputBindingPanel::remove_requested,this,&ControlAssignPopup::on_output_remove);
	connect(p,&OutputBindingPanel::changed,this,&ControlAssignPopup::mark_dirty);
	on_output_expand(idx); mark_dirty();
}

void ControlAssignPopup::on_learn_clicked() {
	if(!m_adapter)return;
	if(m_adapter->is_learning()){m_adapter->cancel_learn();return;}
	m_adapter->start_learn(m_port_id);
	m_learn_btn->setText("⏳ Listening..."); m_status_label->setText("Move a MIDI control...");
}
void ControlAssignPopup::on_binding_learned(const MidiPortBinding &b) {
	m_learn_btn->setText("🎹 Learn");
	if(m_panels.isEmpty()) on_add_clicked();
	int t=m_active_panel>=0?m_active_panel:0;
	if(t<m_panels.size()) m_panels[t]->set_learned_source(b.device_index,b.channel,b.data1,b.is_encoder,b.encoder_mode,b.encoder_sensitivity);
	// Find device name for status label
	QString dev_name = (b.device_index >= 0 && b.device_index + 1 < m_cached_in_devices.size())
		? m_cached_in_devices[b.device_index + 1] : "Any";
	m_status_label->setText(QString("Learned: %1 Ch%2 CC%3").arg(dev_name).arg(b.channel).arg(b.data1));
	mark_dirty();
}
void ControlAssignPopup::on_learn_cancelled() { m_learn_btn->setText("🎹 Learn"); m_status_label->setText("Learn cancelled"); }

void ControlAssignPopup::on_apply_clicked() {
	if(!m_adapter)return;
	m_adapter->remove_binding(m_port_id);
	for(auto *p:m_panels) m_adapter->add_binding(p->build_binding(m_port_id));
	m_adapter->remove_output(m_port_id);
	for(auto *p:m_output_panels) m_adapter->add_output(p->build(m_port_id));
	m_status_label->setText("Applied"); mark_clean();
}

void ControlAssignPopup::on_panel_expand(int i) {
	bool was_expanded = m_panels[i]->is_expanded();
	for(int j=0;j<m_panels.size();j++) m_panels[j]->set_expanded(false);
	if (!was_expanded) {
		m_panels[i]->set_expanded(true);
		m_active_panel=i;
	} else {
		m_active_panel=i; // keep tracking even when collapsed
	}
}
void ControlAssignPopup::on_panel_remove(int i) {
	if(i<0||i>=m_panels.size())return;
	auto *p=m_panels.takeAt(i); m_panel_layout->removeWidget(p); p->deleteLater();
	for(int j=0;j<m_panels.size();j++) m_panels[j]->set_index(j);
	if(m_active_panel>=m_panels.size()) m_active_panel=m_panels.size()-1;
	mark_dirty();
}
void ControlAssignPopup::on_output_expand(int i) {
	for(int j=0;j<m_output_panels.size();j++) m_output_panels[j]->set_expanded(j==i);
	m_active_output=i;
}
void ControlAssignPopup::on_output_remove(int i) {
	if(i<0||i>=m_output_panels.size())return;
	auto *p=m_output_panels.takeAt(i); m_output_layout->removeWidget(p); p->deleteLater();
	for(int j=0;j<m_output_panels.size();j++) m_output_panels[j]->set_index(j);
	mark_dirty();
}

void ControlAssignPopup::on_raw_midi(int device, int status, int data1, int data2) {
	// Monitor always shows all messages if visible
	if(m_monitor_container->isVisible()){
		if(m_monitor_msg_count>500) m_monitor_log->clear();
		m_monitor_log->appendPlainText(QString("[%1] d1=%2 d2=%3 dev=%4").arg(status,2,16).arg(data1).arg(data2).arg(device));
		m_monitor_msg_count++;
	}
	int mt = status & 0xF0;
	int channel = status & 0x0F;
	if(mt==0xB0 && m_active_panel>=0 && m_active_panel<m_panels.size()) {
		// Sync preview params for source matching (also syncs for pipeline run below)
		auto *panel = m_panels[m_active_panel];
		panel->sync_preview_params();
		auto &ps = panel->preview_state();
		bool device_match = (ps.device_index == -1) || (ps.device_index == device);
		bool source_match = device_match && (channel == ps.channel) && (data1 == ps.data1);
		if(!source_match) return;

		m_master_preview->pulse_input();
		m_master_preview->set_raw_midi(data2);
		m_last_raw = data2;
		panel->pulse_header_activity();
		double val = panel->update_pipeline_preview(data2);
		m_master_preview->set_value(val);
		if (m_pipeline_visual) m_pipeline_visual->feed(data2, panel->last_preview());
	}
}

void ControlAssignPopup::on_preview_tick() {
	if (m_active_panel < 0 || m_active_panel >= m_panels.size()) return;
	auto *panel = m_panels[m_active_panel];
	// Only re-evaluate when time-based stages need convergence (Smooth, AnimateTo, etc.)
	if (!panel->needs_preview_convergence()) return;
	double val = panel->update_pipeline_preview(m_last_raw);
	m_master_preview->set_value(val);
	if (m_pipeline_visual)
		m_pipeline_visual->feed(m_last_raw, panel->last_preview());
}

void ControlAssignPopup::refresh_preview() {
	if (m_active_panel < 0 || m_active_panel >= m_panels.size()) return;
	auto *panel = m_panels[m_active_panel];
	double val = panel->update_pipeline_preview(m_last_raw);
	m_master_preview->set_value(val);
	if (m_pipeline_visual)
		m_pipeline_visual->feed(m_last_raw, panel->last_preview());
}

void ControlAssignPopup::sync_ui_state() {
	if (m_active_panel < 0 || m_active_panel >= m_panels.size()) return;
	auto *panel = m_panels[m_active_panel];
	double val = panel->sync_pipeline_state(m_last_raw);
	m_master_preview->set_static_value(val);
	if (m_pipeline_visual)
		m_pipeline_visual->set_static(m_last_raw, panel->last_preview());
}

void ControlAssignPopup::toggle_monitor(bool e) {
	m_monitor_container->setVisible(e);
	m_monitor_toggle->setText(e ? "MIDI Monitor ▼" : "MIDI Monitor ▶");
}

void ControlAssignPopup::show_near(QWidget *target) {
	if(!target){show();return;}
	auto tl=target->mapToGlobal(QPoint(target->width()+8,0));
	auto screen=QApplication::screenAt(tl);
	if(screen){
		auto sr=screen->availableGeometry();
		if(tl.x()+width()>sr.right()) tl.setX(target->mapToGlobal(QPoint(0,0)).x()-width()-8);
		if(tl.y()+height()>sr.bottom()) tl.setY(sr.bottom()-height());
		if(tl.y()<sr.top()) tl.setY(sr.top());
	}
	move(tl); show();
}

} // namespace super
