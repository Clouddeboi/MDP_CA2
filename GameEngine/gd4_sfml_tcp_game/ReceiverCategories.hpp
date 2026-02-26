#pragma once
enum class ReceiverCategories
{
	kNone = 0,
	kScene = 1 << 0,
	kAlliedAircraft = 1 << 2,
	kEnemyAircraft = 1 << 3,
	kAlliedProjectile = 1 << 4,
	kEnemyProjectile = 1 << 5,
	kPickup = 1 << 6,
	kParticleSystem = 1 << 7,
	kSoundEffect = 1 << 8,
	kPlatform = 1 << 9,
	kPlayer1 = 1 << 10,
	kPlayer2 = 1 << 11,
	kPlayer3 = 1 << 12,
	kPlayer4 = 1 << 13,
	kPlayer5 = 1 << 14,
	kPlayer6 = 1 << 15,
	kPlayer7 = 1 << 16,
	kPlayer8 = 1 << 17,
	kPlayer9 = 1 << 18,
	kPlayer10 = 1 << 19,
	kPlayer11 = 1 << 20,
	kPlayer12 = 1 << 21,
	kPlayer13 = 1 << 22,
	kPlayer14 = 1 << 23,
	kPlayer15 = 1 << 24,
	kPlayer16 = 1 << 25,
	kPlayer17 = 1 << 26,
	kPlayer18 = 1 << 27,
	kPlayer19 = 1 << 28,
	kPlayer20 = 1 << 29,
	kPlayerAircraft = kPlayer1 | kPlayer2 | kPlayer3 | kPlayer4 | kPlayer5 | kPlayer6 | kPlayer7 | kPlayer8 | kPlayer9 | kPlayer10 | kPlayer11 | kPlayer12 | kPlayer13 | kPlayer14 | kPlayer15 | kPlayer16 | kPlayer17 | kPlayer18 | kPlayer19 | kPlayer20,
	kBox = 1 << 30,

	kAircraft = kPlayerAircraft | kAlliedAircraft | kEnemyAircraft,
	kProjectile = kAlliedProjectile | kEnemyProjectile
};

ReceiverCategories GetPlayerCategory(int player_id);

// A message would be sent to all aircraft
//unsigned int all_aircraft = ReceiverCategories::kPlayerAircraft | ReceiverCategories::kAlliedAircraft | ReceiverCategories::kEnemyAircraft