#include "material.h"

std::unordered_map<Tmpl8::Surface*, int> raytracer::Material::s_texturesMap =
	std::unordered_map<Tmpl8::Surface*, int>();
std::vector<Tmpl8::Surface*> raytracer::Material::s_textures = std::vector<Tmpl8::Surface*>();