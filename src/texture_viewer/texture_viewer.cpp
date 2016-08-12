#include "../ext/imgui/imgui.h"
#include "../ext/imgui-sfml/imgui-SFML.h"
#include <SFML/Graphics.hpp>

#include <string>
#include <fstream>

enum Endianness : int {
	TV_LITTLE_ENDIAN = 0,
	TV_BIG_ENDIAN = 1
};

const char *endianness_names[] = {"Little endian", "Big endian"};

struct TextureData {
	char file_path[4096] = {0};
	int columns  = 0;
	int lines    = 0;
	int offset   = 0;
	int bpp      = 0;

	int line_byte_skip = 0;
	int pixel_byte_skip = 0;

	Endianness endianness = Endianness::TV_LITTLE_ENDIAN;

	sf::Image image;
	sf::Texture tex;

	std::vector<uint8_t> data;

	const int get_pixel_size() const { return bpp + pixel_byte_skip; }
	const int get_pixel_line_size() const { return get_pixel_size() * columns; }
	const int get_line_size() const { return get_pixel_line_size() + line_byte_skip; }
	const int get_frame_size() const { return get_line_size() * lines; }

	sf::Color get_pixel(int x, int y) {
		const int line_offset = y * get_pixel_line_size();
		const int pixel_offset = x * get_pixel_size();
		const int pixel_pos = line_offset + pixel_offset;

		uint8_t pixel_bytes[16] = {0}; // max pixel bytes value

		for (int i = 0; i < bpp; ++i) {
			pixel_bytes[endianness == Endianness::TV_LITTLE_ENDIAN ? bpp - 1 - i : i] = data[pixel_pos + i];
		}

		sf::Color result;

		switch (bpp) {
		case 1: {
			result.r = result.g = result.b = pixel_bytes[0];
			result.a = 255;
		} break;
		case 2: {
			result.r = ((pixel_bytes[0] & 0xF8) >> 3) << 3;
			result.g = (((pixel_bytes[0] & 0x07) << 2) | ((pixel_bytes[1] & 0xC0) >> 6)) << 3;
			result.b = ((pixel_bytes[1] & 0x3E) >> 1) << 3;
			result.a = 255;
			if (pixel_bytes[1] & 0x0) {
				result.r /= 2;
				result.g /= 2;
				result.b /= 2;
			}
		} break;
		case 3: {
			result.r = pixel_bytes[0];
			result.g = pixel_bytes[1];
			result.b = pixel_bytes[2];
			result.a = 255;
		} break;
		case 4: {
			result.r = pixel_bytes[0];
			result.g = pixel_bytes[1];
			result.b = pixel_bytes[2];
			result.a = pixel_bytes[3];
		} break;
		default: break;
		}

		return result;
	}

	void update() {
		if (bpp == 0) 
			return;

		std::ifstream f(file_path, std::ios::binary);
		if ( ! f.good() || ! f.is_open())
			return;

		data.resize(lines * get_pixel_line_size());

		std::vector<uint8_t> line_buffer(get_line_size(), 0x33);

		f.seekg(offset);

		for (int y = 0; (y < lines) && ( ! f.eof()); ++y) {
			f.read(reinterpret_cast<char*>(line_buffer.data()), get_line_size());
			std::copy(line_buffer.begin(), line_buffer.begin() + get_pixel_line_size(), data.begin() + (y * get_pixel_line_size()));
		}

		f.close();

		image.create(columns, lines, sf::Color::Magenta);

		for (int y = 0; y < lines; ++y) {
			for (int x = 0; x < columns; ++x) {
				image.setPixel(x, y, get_pixel(x,y));
			}
		}

		tex.loadFromImage(image);
	}
};

int main() {
	sf::RenderWindow window{{800, 600}, "texture viewer"};
	window.setFramerateLimit(60);
	ImGui::SFML::Init(window);

	TextureData tex_data;

	int offset_step = 1;
	int scale[2] = {1, 1};

	sf::Clock frame_timer;
	while (window.isOpen()) {
		sf::Time frame_time = frame_timer.restart();

		sf::Event event;
		while (window.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);

			switch (event.type) {
			case sf::Event::Closed: {
				window.close();
			} break;
			default: break;
			}
		}

		ImGui::SFML::Update(frame_time);

		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize({window.getSize().x, window.getSize().y});
		if (ImGui::Begin("Texture Viewer", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {

			if (ImGui::InputText("Path to ROM", tex_data.file_path, sizeof(tex_data.file_path))) {
				tex_data.update();
			}

			int endianness = tex_data.endianness;
			if (ImGui::Combo("Endianness", &endianness, endianness_names, 2)) {
				tex_data.endianness = static_cast<Endianness>(endianness);
				tex_data.update();
			}

			if (ImGui::InputInt("Offset Step", &offset_step, 1, ImGuiInputTextFlags_CharsHexadecimal)) {
				tex_data.update();
			}

			if (ImGui::InputInt("Offset", &tex_data.offset, offset_step, offset_step, ImGuiInputTextFlags_CharsHexadecimal)) {
				tex_data.update();
			}

			if (ImGui::InputInt("Columns", &tex_data.columns, 1, 8)) {
				tex_data.update();
			}

			if (ImGui::InputInt("Lines", &tex_data.lines, 1, 8)) {
				tex_data.update();
			}

			if (ImGui::InputInt("Bytes per Pixel", &tex_data.bpp)) {
				tex_data.update();
			}

			if (ImGui::InputInt("Line byte skip", &tex_data.line_byte_skip)) {
				tex_data.update();
			}

			if (ImGui::InputInt("Pixel byte skip", &tex_data.pixel_byte_skip)) {
				tex_data.update();
			}

			if (ImGui::Button("Prev line")) {
				tex_data.offset -= tex_data.get_line_size();
				tex_data.update();
			}

			ImGui::SameLine();

			if (ImGui::Button("Next line")) {
				tex_data.offset += tex_data.get_line_size();
				tex_data.update();
			}

			ImGui::SameLine();

			ImGui::Text("Line size: %d", tex_data.get_line_size());

			if (ImGui::Button("Prev frame")) {
				tex_data.offset -= tex_data.get_frame_size();
				tex_data.update();
			}

			ImGui::SameLine();

			if (ImGui::Button("Next frame")) {
				tex_data.offset += tex_data.get_frame_size();
				tex_data.update();
			}

			ImGui::InputInt2("Scale", scale);

			ImGui::Image(tex_data.tex, {tex_data.columns * scale[0], tex_data.lines * scale[1]}, sf::Color::White, sf::Color::Magenta);

			ImGui::End();
		}	

		window.clear();

		ImGui::Render();

		window.display();
	}

	ImGui::SFML::Shutdown();
}
