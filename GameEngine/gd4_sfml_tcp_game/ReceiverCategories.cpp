#include "ReceiverCategories.hpp"

ReceiverCategories GetPlayerCategory(int player_id)
{
	switch (player_id)
	{
	case 0: return ReceiverCategories::kPlayer1;
	case 1: return ReceiverCategories::kPlayer2;
	case 2: return ReceiverCategories::kPlayer3;
	case 3: return ReceiverCategories::kPlayer4;
	case 4: return ReceiverCategories::kPlayer5;
	case 5: return ReceiverCategories::kPlayer6;
	case 6: return ReceiverCategories::kPlayer7;
	case 7: return ReceiverCategories::kPlayer8;
	case 8: return ReceiverCategories::kPlayer9;
	case 9: return ReceiverCategories::kPlayer10;
	case 10: return ReceiverCategories::kPlayer11;
	case 11: return ReceiverCategories::kPlayer12;
	case 12: return ReceiverCategories::kPlayer13;
	case 13: return ReceiverCategories::kPlayer14;
	case 14: return ReceiverCategories::kPlayer15;
	case 15: return ReceiverCategories::kPlayer16;
	case 16: return ReceiverCategories::kPlayer17;
	case 17: return ReceiverCategories::kPlayer18;
	case 18: return ReceiverCategories::kPlayer19;
	case 19: return ReceiverCategories::kPlayer20;
	default: return ReceiverCategories::kPlayerAircraft;
	}
}