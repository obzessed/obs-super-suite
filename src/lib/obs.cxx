#include "./obs.hxx"

namespace obs::core {

Canvas::Canvas(obs_canvas_t* canvas) : _inner(canvas)
{
	if (!_inner) {
		throw std::exception("invalid canvas reference");
	}
}

Canvas Canvas::getMain()
{
	const auto inner = obs_get_main_canvas();
	return Canvas(inner);
}

std::optional<Canvas> Canvas::findByName(const std::string &name)
{
	const auto inner = obs_get_canvas_by_name(name.c_str());
	if (!inner) return std::nullopt;
	return Canvas(inner);
}

std::optional<Canvas> Canvas::findByUuid(const std::string &uuid)
{
	const auto inner = obs_get_canvas_by_uuid(uuid.c_str());
	if (!inner) return std::nullopt;
	return Canvas(inner);
}

void Canvas::forEach(const std::function<bool(const Canvas&, size_t)>& callback)
{
	struct IterState {
		size_t index = 0;
		decltype(callback) callback;
	};

	auto cb = [](void *param, obs_canvas_t *canvas) -> bool {
		auto *state = static_cast<IterState*>(param);

		const auto wrapped = Canvas(canvas);
		return state->callback(wrapped, state->index++);
	};

	IterState state{0, callback};

	obs_enum_canvases(cb, &state);
}

} // namespace obs
