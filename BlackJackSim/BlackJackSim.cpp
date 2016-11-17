// BlackJackSim.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <vector>
#include <sstream>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <list>
#include <time.h>
#include <cassert>

std::ofstream nullStream;
//std::ostream & output = std::cout;
std::ostream & output = nullStream;
//#define output std::cout;
//#define output 

enum CardFace
{
	Ace,
	Two,
	Three,
	Four,
	Five,
	Six,
	Seven,
	Eight,
	Nine,
	Ten,
	Jack,
	Queen,
	King,
};

enum CardSuit
{
	Spades,
	Hearts,
	Clubs,
	Diamonds,
};


class Card
{
public:
	Card(int cardValue);

	CardFace Face() const;
	CardSuit Suit() const;
	int Value() const;

	bool IsSoft() const;
	std::string ToString() const;

private:
	int m_rawValue;
};

Card::Card(int cardValue)
	: m_rawValue(cardValue)
{
	
}

CardFace Card::Face() const
{
	return static_cast<CardFace>(m_rawValue % 13);
}

CardSuit Card::Suit() const
{
	return static_cast<CardSuit>(m_rawValue / 13);
}

int Card::Value() const
{
	if (Face() == Ace)
		return 11;
	else if (Face() >= Ten)
		return 10;
	else
		return (static_cast<int>(Face()) + 1); // +1 due to zero based enumeration
}


std::string Card::ToString() const
{
	char* szSuitNames[4] = {"S", "H", "C", "D"};
	char* szFaceNames[13] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

	std::string strFaceName = szFaceNames[static_cast<int>(Face())];
	std::string strSuitName = szSuitNames[static_cast<int>(Suit())];

	return strFaceName + strSuitName;
}

class Player
{
public:
	Player(std::string && name, double initialMoney)
		: m_name(std::move(name))
		, m_money(initialMoney)
		, m_hands(0)
	{ }

	const std::string & Name() { return m_name; }
	double Money() { return m_money; }
	void AdjustMoney(double amount) { m_money += amount; }
	void SignalNewHand() { m_hands++; }
	int Hands() const { return m_hands; }
	void ClearStats() { m_money = 0; m_hands = 0; }

protected:
	std::string m_name;
	double m_money;
	int m_hands;
};



class DeckShoe
{
public:
	DeckShoe(int deckCount);

	Card DealCard();
	void ReloadIfNecessary();

private:
	void Clear();
	void LoadDecks(int deckCount);
	void Shuffle();

	int m_decks;
	std::vector<Card> m_cards;
};

DeckShoe::DeckShoe(int deckCount)
	: m_decks(deckCount)
{
	LoadDecks(m_decks);
	Shuffle();
}

void DeckShoe::Clear()
{
	m_cards.clear();
}

void DeckShoe::LoadDecks(int deckCount)
{
	for (int i = 0; i < deckCount; ++i)
	{
		for (int card = 0; card < 52; card++)
		{
			m_cards.emplace_back(card);
		}
	}
}

void DeckShoe::Shuffle()
{
	std::random_shuffle(begin(m_cards), end(m_cards));
}

Card DeckShoe::DealCard()
{
	Card returnCard = m_cards.back();
	m_cards.pop_back();
	return returnCard;
}

void DeckShoe::ReloadIfNecessary()
{
	double c_penetration = 0.3;
	if (m_cards.size() < (size_t)(c_penetration * 52 * m_decks))
	{
		Clear();
		LoadDecks(m_decks);
		Shuffle();
	}
}


class Hand
{
public:
	void AddCard(Card card);

	int          Value() const;
	bool         IsBusted() const { return Value() > 21; }
	bool         IsSoft() const;
	bool         IsBlackjack() const;
	Card         GetCard(int i) const { return m_cards[i]; }

	std::string  ToString();

protected:
	std::pair<int, bool> ComputeValue() const;  // sum, isSoft

	std::vector<Card> m_cards;
};


void Hand::AddCard(Card card)
{
	m_cards.emplace_back(card);
}

std::pair<int, bool> Hand::ComputeValue() const  // <sum, isSoft
{
	int sum = 0;
	int aceCount = 0;

	for (const auto & card : m_cards)
	{
		sum += card.Value();
		if (card.Face() == Ace)
			aceCount++;
	}

	// Adjust aces so they don't 'bust' us
	while (aceCount > 0 && sum > 21)
	{
		aceCount--;
		sum -= 10;
	}

	return std::make_pair(sum, aceCount > 0);
}

int Hand::Value() const
{
	return ComputeValue().first;
}

bool Hand::IsSoft() const
{
	return ComputeValue().second;
}

bool Hand::IsBlackjack() const
{
	return m_cards.size() == 2 && Value() == 21;
}


std::string Hand::ToString()
{
	std::ostringstream oss;

	bool isFirstCard = true;

	for (const auto & card : m_cards)
	{
		if (!isFirstCard)
			oss << ", ";
		isFirstCard = false;

		oss << card.ToString();
	}

	return oss.str();
}

class DealerHand : public Hand
{
public:
	DealerHand()
		: m_isFirstCardHidden(true)
	{ }

	int Showing();
	void FlipHiddenCard() { m_isFirstCardHidden = false; }
	std::string ToString();

protected:
	bool m_isFirstCardHidden;
};

int DealerHand::Showing()
{
	return m_cards[1].Value();
}

std::string DealerHand::ToString()
{
	if (m_isFirstCardHidden)
		return m_cards[1].ToString();
	else
		return __super::ToString();
}

class PlayerHand : public Hand
{
public:
	PlayerHand(Player & player)
		: m_player(player)
		, m_bet(1.0)
		, m_isFromSplit(false)
	{

	}

	bool CanHit();
	bool CanSplit() const;
	void DoubleDown();
	bool CanDoubleDown() const { return m_cards.size() == 2; }
	PlayerHand Split(DeckShoe & shoe);

	Player& Owner() { return m_player; }
	std::string PlayerName() const { return m_player.Name(); }
	double Bet() { return m_bet; }
	void PayoutHand(double result);

private:
	Player&  m_player;
	double   m_bet;
	bool     m_isFromSplit;
};

bool PlayerHand::CanSplit() const
{
	return m_cards.size() == 2 && m_cards[0].Face() == m_cards[1].Face();
}

void PlayerHand::DoubleDown()
{
	assert(m_cards.size() == 2);
	m_bet *= 2;
}

PlayerHand PlayerHand::Split(DeckShoe & shoe)
{
	PlayerHand newHand(Owner());

	assert(CanSplit());

	newHand.AddCard(m_cards[1]);
	m_cards.pop_back();

	newHand.AddCard(shoe.DealCard());
	AddCard(shoe.DealCard());

	return newHand;
}

void PlayerHand::PayoutHand(double result)
{
	m_player.AdjustMoney(m_bet * result);
}

bool PlayerHand::CanHit()
{
	bool cantHit = IsBusted() || IsBlackjack() || (m_isFromSplit && m_cards[0].Face() == Ace) || Value() >= 21;
	return !cantHit;
}


double GetHandOutcome(const PlayerHand & playerHand, const Hand & dealerHand)
{
	output << playerHand.PlayerName() << ": ";
	if (playerHand.IsBusted())
		return output << "Busted\n", -1;
	else if (playerHand.IsBlackjack() && dealerHand.IsBlackjack())
		return output << "Pushed\n", 0;
	else if (dealerHand.IsBlackjack())
		return output << "Lost\n", -1;
	else if (playerHand.IsBlackjack())
		return output << "Blackjack!\n", 1.5;
	else if (dealerHand.IsBusted() || playerHand.Value() > dealerHand.Value())
		return output << "Won!\n", 1.0;
	else if (playerHand.Value() == dealerHand.Value())
		return output << "Pushed!\n", 0.0;
	else
		return output << "Lost!\n", -1.0;
}

enum class Action
{
	Stand,
	Hit,
	DoubleDown,
	Split
};

class ActionTable
{
public:
	Action GetAction(int dealerIndex, int playerIndex);

	void GenerateRandomActionTable();
	void AdjustRandomActionTable(int entriesToAdjust);
	void LoadActionTable(const std::string & strTable);
	std::string SaveActionTable();
	void PrintActionTable(std::ostream & stream);

private:

	Action m_actions[34][10];
	int m_actionCount[34][10];
};

Action ActionTable::GetAction(int dealerIndex, int playerIndex)
{
	m_actionCount[playerIndex][dealerIndex]++;
	return m_actions[playerIndex][dealerIndex];
}

void ActionTable::GenerateRandomActionTable()
{
	for (int i=0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			int r = rand() % 4;
			m_actions[i][j] = static_cast<Action>(r);
		}
	}

	for (int i=10; i < 34; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			int r = rand() % 3; // no split
			m_actions[i][j] = static_cast<Action>(r);
		}
	}

	for (int i=0; i < 34; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			m_actionCount[i][j] = 0;
		}
	}
}

void ActionTable::AdjustRandomActionTable(int entriesToAdjust)
{
	for (int e = 0; e < entriesToAdjust; e++)
	{
		int i = rand() % 34;
		int j = rand() % 10;

		int r = 0;
		if (i < 10)
			r = rand() % 4;
		else
			r = rand() % 3;

		m_actions[i][j] = static_cast<Action>(r);
	}

	for (int i=0; i < 34; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			m_actionCount[i][j] = 0;
		}
	}
}


void ActionTable::LoadActionTable(const std::string & strActions)
{
	int index=0;

	for (int i=0; i < 34; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			char c = strActions[index++];
			switch (c)
			{
			case 's': m_actions[i][j] = Action::Stand; break;
			case 'h': m_actions[i][j] = Action::Hit; break;
			case 'd': m_actions[i][j] = Action::DoubleDown; break;
			case 'p': m_actions[i][j] = Action::Split; break;
			}
		}
	}

}

std::string ActionTable::SaveActionTable()
{
	std::string strActions;
	strActions.reserve(34*10);

	for (int i=0; i < 34; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			switch (m_actions[i][j])
			{
			case Action::Stand: strActions.append(1, 's'); break;
			case Action::Hit: strActions.append(1, 'h'); break;
			case Action::DoubleDown: strActions.append(1, 'd'); break;
			case Action::Split: strActions.append(1, 'p'); break;
			}
		}
	}

	return std::move(strActions);
}

void ActionTable::PrintActionTable(std::ostream & stream)
{
	for (int i=0; i < 34; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			switch (m_actions[i][j])
			{
			case Action::Stand: stream << "s "; break;
			case Action::Hit: stream << "h "; break;
			case Action::DoubleDown: stream << "d "; break;
			case Action::Split: stream << "p "; break;
			}
			stream << "(" << m_actionCount[i][j] << ") ";
		}
		stream << std::endl;
	}
	stream << std::endl;
}


// Player actions:
//  Double A - 10   (10)
//  Soft 13 - 20   (8)
//  5 - 20    (16)
int MapPlayerHandToActionIndex(PlayerHand & hand)
{
	if (hand.CanSplit() && hand.GetCard(0).Value() == 11)
		return 0;
	else if (hand.CanSplit())
		return hand.GetCard(0).Value() - 1;
	else if (hand.IsSoft())
		return 10 + hand.Value() - 13;
	else
	{
		assert(hand.Value() >= 5 && hand.Value() < 21);
		return 18 + hand.Value() - 5;
	}
}

// DealerHand actions:
//  2-10 = 0-8
//  A = 9
int MapDealerHandToActionIndex(int dealerCardValue)
{
	if (dealerCardValue == 1)
		return 9;
	else
		return dealerCardValue - 2;
}

std::string GetNextAction(DealerHand & dealer, PlayerHand & player)
{
	return "s";
}

int s_Results[5] = {0,0,0,0,0};

bool RunOneRound(DeckShoe & shoe, std::vector<Player> & players, ActionTable & actionTable)
{
	DealerHand dealerHand;
	std::list<PlayerHand> playerHands;

	shoe.ReloadIfNecessary();

	for (auto & player : players)
	{
		playerHands.emplace_back(player);
	}

	for (auto & hand : playerHands)
	{
		hand.AddCard(shoe.DealCard());
	}
	dealerHand.AddCard(shoe.DealCard());

	for (auto & hand : playerHands)
	{
		hand.AddCard(shoe.DealCard());
	}
	dealerHand.AddCard(shoe.DealCard());

	//output << "*******************************************" << std::endl;
	for (auto & hand : playerHands)
	{
		//output << "Dealer showing: " << dealerHand.ToString() << " (" << dealerHand.Showing() << ")" << std::endl;
		//output << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;

		while (hand.CanHit() && !dealerHand.IsBlackjack())
		{
			Action action;
			//output << "Action (h, s, p, d)? ";
			
			//std::cin >> action;
			action = actionTable.GetAction(MapDealerHandToActionIndex(dealerHand.Showing()), MapPlayerHandToActionIndex(hand));
			//action = GetNextAction(dealerHand, hand);

			if (action == Action::DoubleDown && !hand.CanDoubleDown())
				action = Action::Hit;

			if (action == Action::Hit)
			{
				//output << "Hitting\n";
				hand.AddCard(shoe.DealCard());
				//output << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
			}
			else if (action == Action::Stand)
			{
				//output << "Standing\n";
				break;
			}
			else if (action == Action::Split && hand.CanSplit())
			{
				// TODO
				playerHands.emplace_back(hand.Split(shoe));
				//output << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
			}
			else if (action == Action::Split)
			{
				std::cerr << "Hand not split-able" << std::endl;
			}
			else if (action == Action::DoubleDown)
			{
				hand.DoubleDown();
				hand.AddCard(shoe.DealCard());
				//output << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
				break;
			}
			//else if (action == "q")
			//{
			//	return false;
			//}
			else
			{
				std::cerr << "Unrecognized command: " << (int)action << std::endl;
			}
		}
	}

	// Todo: check for all players busted
	dealerHand.FlipHiddenCard();

	//output << "Dealer: " << dealerHand.ToString() << " (" << dealerHand.Value() << ")" << std::endl;

	while (dealerHand.Value() < 17 || (dealerHand.Value() == 17 && dealerHand.IsSoft()))
	{
		auto card = shoe.DealCard();
		dealerHand.AddCard(card);
		//output << "Dealer: " << dealerHand.ToString() << " (" << dealerHand.Value() << ")" << std::endl;
	}

	for (auto & player : players)
	{
		player.SignalNewHand();
	}

	for (auto & hand : playerHands)
	{
		//output << std::endl;
		//output << hand.PlayerName() <<  "'s final hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
		double outcome = GetHandOutcome(hand, dealerHand);

		//if (-1.0 == outcome)
		//	s_Results[0]++;
		//else if (0.0 == outcome)
		//	s_Results[1]++;
		//else if (1.0 == outcome)
		//	s_Results[2]++;
		//else if (1.5 == outcome)
		//	s_Results[3]++;

		hand.PayoutHand(outcome);
		//output << "Payout: " << hand.Bet() * outcome << " (" << hand.Owner().Money() << ")" << std::endl;
	}

	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	srand(static_cast<unsigned int>(time(NULL)));
	DeckShoe shoe(6);
	Player dealer(std::string("Dealer"), 0);
	
	std::vector<Player> players;

	ActionTable bestActionTableSoFar;
	double bestExpectedValue = -100;

	players.emplace_back("Player 1", 0.0);
	//players.emplace_back("Player 2", 500.0);


	bool fRunMore = true;

	bestActionTableSoFar.LoadActionTable(
		"sdphsddspdppdhhdpdpppppsspphhdpdpspshdhpsshppphpdddsshdhphhhhspshpspshhhshhhpphspdpspshspssspspssspdhsshddhdhhhddhshhddhdhhhsssdhhdsddddhdhdddsdhhhdssshdhsdhhshdhdshsdddssshddhssshsshddhsdshhshshddhhdhhsdshshhsdhsdddhdshssddhhdhhdhhhhdddhdddddddhdddhddssssdshsssdhssshhhssshdshssdssshsshssdsshhsssdshssshhhsssssshhsssssdssssssssshssssssssss"
		
		/*card */// "ppppppppppppdphsssssphpssssssspdpspsssssddddddddssdpshdhphhhhspshpspshppppppppppddpspshppssssssssssshssddhhdhhhddhshdddhdhhhsssddhdsddddddhdddsdhhhdssdhdhhdhhdhsssssssssssssssssssshhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhddddhhhhhddddddddhhdddddddddhhhsssshhhhsssssshhhhsssssshhhhsssssshhhhsssssshhhhssssssssssssssssssssssssssssssssssssssss"
		);
	//bestActionTableSoFar.GenerateRandomActionTable();

	//while (fRunMore)
	for (int round = 0; round != 1 /*200*/; round++)
	{
		players[0].ClearStats();
		ActionTable actionTable;

		if (round % 50 == 0)
			std::cout << "Round " << round << std::endl;
		
		actionTable = bestActionTableSoFar;

		if (round > 10)
			actionTable.AdjustRandomActionTable(5);

		for (int i=0; i < 100000; i++)
		{
			fRunMore = RunOneRound(shoe, players, actionTable);
		}

		double expectedValue = players[0].Money() / players[0].Hands();
		if (expectedValue > bestExpectedValue)
		{
			bestActionTableSoFar = actionTable;
			bestExpectedValue = expectedValue;
			std::cout << "Best expected value (round " << round << "): " << bestExpectedValue << "\n";
		}
	}

	std::cout << "Best expected value: " << bestExpectedValue << "\n";
	//bestActionTableSoFar.PrintActionTable(std::cout);

	//do
	//{
	//	fRunMore = RunOneRound(shoe, players);
	//	output << std::endl << std::endl;
	//} while (fRunMore);

	//for (auto & player : players)
	//{
	//	std::cout << player.Name() << ": Final money = " << player.Money() << std::endl;
	//	std::cout << " Expected value: " << player.Money() / player.Hands() << std::endl;
	//}

	std::cout << bestActionTableSoFar.SaveActionTable() << std::endl;

	return 0;
}

