// ============================================================================
// Animation System — TweenManager port helper implementation
// ============================================================================

#include "animation.hpp"
#include "control_port.hpp"

namespace super {

int TweenManager::animate_port(ControlPort *port, double target,
								int duration_ms, QEasingCurve::Type curve)
{
	if (!port)
		return -1;

	double from = port->as_double();

	return animate(from, target, duration_ms,
		[port](double val) {
			port->set_value(QVariant(val));
		},
		curve);
}

} // namespace super
