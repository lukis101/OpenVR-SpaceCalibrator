#include "stdafx.h"
#include "Configuration.h"

#include <picojson.h>

#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>

static void UpgradeProfileV1(CalibrationContext &ctx);
static void ParseProfileV2(CalibrationContext &ctx, std::istream &stream);

static std::string ConfigFileName()
{
	std::string vrRuntimeConfigName = vr::VR_RuntimePath();
	return vrRuntimeConfigName + "\\..\\..\\..\\config\\01spacecalibrator\\calibration.json";
}

void LoadProfile(CalibrationContext &ctx)
{
	ctx.validProfile = false;

	std::ifstream file(ConfigFileName());
	if (!file.good())
	{
		std::cout << "Profile missing, trying fallback path" << std::endl;
		// Fallback to working directory, which was the default for a long time.
		file = std::ifstream("openvr_space_calibration.json");
	}


	if (!file.good())
	{
		std::cout << "Fallback profile missing, trying V1 path" << std::endl;
		UpgradeProfileV1(ctx);
		return;
	}

	try
	{
		ParseProfileV2(ctx, file);
		std::cout << "Loaded profile" << std::endl;
	}
	catch (const std::runtime_error &e)
	{
		std::cerr << "Error loading profile: " << e.what() << std::endl;
	}
}

static void ParseProfileV2(CalibrationContext &ctx, std::istream &stream)
{
	picojson::value v;
	std::string err = picojson::parse(v, stream);
	if (!err.empty())
		throw std::runtime_error(err);

	auto arr = v.get<picojson::array>();
	if (arr.size() < 1)
		throw std::runtime_error("no profiles in file");

	auto obj = arr[0].get<picojson::object>();

	ctx.referenceTrackingSystem = obj["reference_tracking_system"].get<std::string>();
	ctx.targetTrackingSystem = obj["target_tracking_system"].get<std::string>();
	ctx.calibratedRotation(0) = obj["roll"].get<double>();
	ctx.calibratedRotation(1) = obj["yaw"].get<double>();
	ctx.calibratedRotation(2) = obj["pitch"].get<double>();
	ctx.calibratedTranslation(0) = obj["x"].get<double>();
	ctx.calibratedTranslation(1) = obj["y"].get<double>();
	ctx.calibratedTranslation(2) = obj["z"].get<double>();

	ctx.validProfile = true;
}

void SaveProfile(CalibrationContext &ctx)
{
	std::cout << "Saving profile to " << ConfigFileName() << std::endl;

	std::ofstream file(ConfigFileName());

	picojson::object profile;
	profile["reference_tracking_system"].set<std::string>(ctx.referenceTrackingSystem);
	profile["target_tracking_system"].set<std::string>(ctx.targetTrackingSystem);
	profile["roll"].set<double>(ctx.calibratedRotation(0));
	profile["yaw"].set<double>(ctx.calibratedRotation(1));
	profile["pitch"].set<double>(ctx.calibratedRotation(2));
	profile["x"].set<double>(ctx.calibratedTranslation(0));
	profile["y"].set<double>(ctx.calibratedTranslation(1));
	profile["z"].set<double>(ctx.calibratedTranslation(2));

	picojson::value profileV;
	profileV.set<picojson::object>(profile);

	picojson::array profiles;
	profiles.push_back(profileV);

	picojson::value profilesV;
	profilesV.set<picojson::array>(profiles);

	file << profilesV.serialize(true);

	ctx.validProfile = true;
}

static void UpgradeProfileV1(CalibrationContext &ctx)
{
	std::ifstream file("openvr_space_calibration.txt");
	ctx.validProfile = false;

	if (!file.good())
		return;

	file
		>> ctx.targetTrackingSystem
		>> ctx.calibratedRotation(1) // yaw
		>> ctx.calibratedRotation(2) // pitch
		>> ctx.calibratedRotation(0) // roll
		>> ctx.calibratedTranslation(0) // x
		>> ctx.calibratedTranslation(1) // y
		>> ctx.calibratedTranslation(2); // z

	ctx.validProfile = true;

	SaveProfile(ctx);

	file.close();
	std::remove("openvr_space_calibration.txt");
}

void WriteActivateMultipleDriversToConfig()
{
	std::string configPath = vr::VR_RuntimePath();
	configPath += "\\..\\..\\..\\config\\steamvr.vrsettings";

	std::ifstream ifile(configPath);
	if (!ifile.good())
		throw std::runtime_error("failed to read steamvr.vrsettings");

	picojson::value v;
	std::string err = picojson::parse(v, ifile);
	if (!err.empty())
		throw std::runtime_error(err);

	ifile.close();

	if (!v.is<picojson::object>())
		throw std::runtime_error("steamvr.vrsettings is empty");

	auto &root = v.get<picojson::object>();

	if (!root["steamvr"].is<picojson::object>())
		throw std::runtime_error("steamvr.vrsettings is missing \"steamvr\" key");

	auto &steamvr = root["steamvr"].get<picojson::object>();

	const bool tru = true; // MSVC picks the wrong specialization when passing a literal...
	steamvr["activateMultipleDrivers"].set<bool>(tru);

	std::ofstream ofile(configPath);
	if (!ofile.good())
		throw std::runtime_error("failed to write steamvr.vrsettings");

	v.serialize(std::ostream_iterator<char>(ofile), true);

	std::cout << "Successfully set activateMultipleDrivers to true" << std::endl;
}
