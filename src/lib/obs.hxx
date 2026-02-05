#pragma once

#include <obs-module.h>

#include <optional>
#include <functional>
#include <string>

// obs api cxx wrapper
namespace obs
{
	namespace core {
		// Serialized Objects
		class Data {};
		class DataArray {};

		class Property {};
		class Properties {};

		// Canvas
		class Canvas {
		private:
			obs_canvas_t* _inner;
		protected:
			explicit Canvas(obs_canvas_t* canvas);
		public:
			static Canvas getMain();

			static std::optional<Canvas> findByName(const std::string &name);

			static std::optional<Canvas> findByUuid(const std::string &uuid);

			static void forEach(const std::function<bool(const Canvas&, size_t)>& callback);
		};

		class WeakCanvas {};

		class Config {};
		class Display {};

		class AudioInfo {};
		class AudioInfo2 {};
		class VideoInfo {};

		class SignalHandler {
		public:
			static SignalHandler& get();
		};
		class SignalCallback {};
		class ProcHandler {
		public:
			static ProcHandler& get();
		};

		class CallData {};

		class AudioResampler {};
		class View {};
		class Encoder {};
		class WeakEncoder {};
		class Output {};
		class WeakOutput {};
		class Audio {};
		class Video {};

		class Service {};
		class WeakService {};

		class Source {};
		class WeakSource {};

		class Scene : public Source {};
		class SceneItem {};

		class Module {};
	}

	namespace frontend {

	}
}
