// BlackJackSim.cpp : Monte Carlo like simulation of black jack to deduce optimal decision tables

#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <list>
#include <random>
#include <sstream>
#include <string>
#include <time.h>
#include <vector>

std::ofstream nullStream;
std::ostream& output = nullStream;     // Easily switch off output
//std::ostream & output = std::cout;   // Or use this one to enable output

#define DebugOut(x)
//#define DebugOut(x) x

enum class CardFace
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

enum class CardSuit
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
	if (Face() == CardFace::Ace)
		return 11;
	else if (Face() >= CardFace::Ten)
		return 10;
	else
		return (static_cast<int>(Face()) + 1); // +1 due to zero based enumeration
}

char* g_szSuitNames[4] = {"S", "H", "C", "D"};
char* g_szFaceNames[13] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

std::string Card::ToString() const
{
	std::string strFaceName = g_szFaceNames[static_cast<int>(Face())];
	std::string strSuitName = g_szSuitNames[static_cast<int>(Suit())];

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

	size_t Size() const { return m_cards.size(); }
	Card GetCard(size_t offset) const { return m_cards[offset]; }

	void Reload();

private:
	void Clear();
	void LoadDecks();
	void Shuffle();

	std::default_random_engine m_randomEngine;
	std::vector<Card> m_cards;
	int m_decks;
};

class DeckShoeView
{
public:
	DeckShoeView(const DeckShoe& deckShoe)
		: m_shoe(deckShoe)
	{ }

	Card DealCard();
	int Offset() const { return m_cardOffset; }
	void SetOffset(int offset) { m_cardOffset = offset; }

protected:
	int m_cardOffset = 0;
	const DeckShoe& m_shoe;
};

class MasterDeckShoeView : public DeckShoeView
{
public:
	MasterDeckShoeView(DeckShoe& deckShoe)
		: DeckShoeView(deckShoe)
		, m_masterShoe(deckShoe)
	{ }

	void ReloadIfNecessary();

private:
	DeckShoe& m_masterShoe;
};

void DeckShoe::Reload()
{
	Clear();
	LoadDecks();
	Shuffle();
}

void MasterDeckShoeView::ReloadIfNecessary()
{
	double c_penetration = 0.7;
	if (m_cardOffset > (c_penetration * m_masterShoe.Size()))
	{
		m_masterShoe.Reload();
		m_cardOffset = 0;
	}
}

Card DeckShoeView::DealCard()
{
	return m_shoe.GetCard(m_cardOffset++);
}


DeckShoe::DeckShoe(int deckCount)
	: m_decks(deckCount)
	, m_randomEngine(std::random_device{}())
{
	LoadDecks();
	Shuffle();
}

void DeckShoe::Clear()
{
	m_cards.clear();
}

void DeckShoe::LoadDecks()
{
	m_cards.reserve(52 * m_decks);
	for (int i = 0; i < m_decks; ++i)
	{
		for (int card = 0; card < 52; card++)
		{
			m_cards.emplace_back(card);
		}
	}
}

void DeckShoe::Shuffle()
{
	std::shuffle(begin(m_cards), end(m_cards), m_randomEngine);
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
	bool         IsFromSplit() const { return m_isFromSplit; }
	void         SetIsFromSplit() { m_isFromSplit = true; }

	std::string  ToString() const;

protected:
	bool     m_isFromSplit = false;

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
		if (card.Face() == CardFace::Ace)
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


std::string Hand::ToString() const
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

	int Showing() const;
	std::string ToString() const;

	void FlipHiddenCard() { m_isFirstCardHidden = false; }

protected:
	bool m_isFirstCardHidden;
};

int DealerHand::Showing() const
{
	return m_cards[1].Value();
}

std::string DealerHand::ToString() const
{
	if (m_isFirstCardHidden)
		return m_cards[1].ToString();
	else
		return __super::ToString();
}

class PlayerSubHand : public Hand
{
public:
	PlayerSubHand(Player& player)
		: m_player(player)
		, m_bet(1.0)
	{ }

	Player& Owner() { return m_player; }

	bool CanHit() const;
	bool CanSplit() const;
	bool CanDoubleDown() const { return m_cards.size() == 2; }
	double Bet() const { return m_bet; }
	const std::string PlayerName() const { return m_player.Name(); }

	void DoubleDown(Card card);
	PlayerSubHand Split(DeckShoeView & shoe);
	void PayoutHand(double result);

private:
	Player&  m_player;
	double   m_bet;
};

class PlayerHand
{
public:
	PlayerHand(Player & player)
		: m_player(player)
		, m_subHands{{player}}
	{
	}

	Player& Owner() { return m_player; }
	std::string PlayerName() const { return m_player.Name(); }

	std::list<PlayerSubHand>& SubHands() { return m_subHands; }
	PlayerSubHand& PrimaryHand() { return m_subHands.front(); }

	bool CanHit() const;

	void AddCard(Card card);
	void Split(PlayerSubHand& subHand, DeckShoeView& shoe);

private:
	Player&  m_player;
	std::list<PlayerSubHand> m_subHands;
};

void PlayerHand::AddCard(Card card)
{
	m_subHands.front().AddCard(card);
}

bool PlayerHand::CanHit() const
{
	for (const auto& subHand : m_subHands)
	{
		if (subHand.CanHit())
			return true;
	}
	return false;
}

void PlayerHand::Split(PlayerSubHand& subHand, DeckShoeView& shoe)
{
	PlayerSubHand newHand = subHand.Split(shoe);
	m_subHands.push_back(std::move(newHand));
}

bool PlayerSubHand::CanSplit() const
{
	return m_cards.size() == 2 && m_cards[0].Face() == m_cards[1].Face();
}

void PlayerSubHand::DoubleDown(Card card)
{
	assert(m_cards.size() == 2);
	m_bet *= 2;
	AddCard(card);
}

PlayerSubHand PlayerSubHand::Split(DeckShoeView & shoe)
{
	PlayerSubHand newHand(Owner());

	assert(CanSplit());

	newHand.AddCard(m_cards[1]);
	m_cards.pop_back();

	newHand.AddCard(shoe.DealCard());
	AddCard(shoe.DealCard());

	SetIsFromSplit();
	newHand.SetIsFromSplit();

	return newHand;
}

void PlayerSubHand::PayoutHand(double result)
{
	m_player.AdjustMoney(m_bet * result);
}

bool PlayerSubHand::CanHit() const
{
	bool cantHit = IsBusted() || IsBlackjack() || (m_isFromSplit && m_cards[0].Face() == CardFace::Ace) || Value() >= 21;
	return !cantHit;
}


double GetHandOutcome(const PlayerSubHand & playerHand, const Hand & dealerHand)
{
	DebugOut(output << playerHand.PlayerName() << ": ");
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

const char* GetActionString(Action action)
{
	switch (action)
	{
		case Action::Stand:
			return "Stand";
		case Action::Hit:
			return "Hit";
		case Action::DoubleDown:
			return "Double DOwn";
		case Action::Split:
			return "Split";
		default:
			return "ERROR";
	}
}

const int c_maxPlayerHandIndex = 31;
const int c_maxDealerHandIndex = 10;

#if 0
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

	Action m_actions[c_maxPlayerHandIndex][c_maxDealerHandIndex];
	int m_actionCount[c_maxPlayerHandIndex][c_maxDealerHandIndex];
};

Action ActionTable::GetAction(int dealerIndex, int playerIndex)
{
	m_actionCount[playerIndex][dealerIndex]++;
	return m_actions[playerIndex][dealerIndex];
}

void ActionTable::GenerateRandomActionTable()
{
	for (int i=0; i < c_maxDealerHandIndex; i++)
	{
		for (int j = 0; j < c_maxDealerHandIndex; j++)
		{
			int r = rand() % 4;
			m_actions[i][j] = static_cast<Action>(r);
		}
	}

	for (int i=c_maxDealerHandIndex; i < c_maxPlayerHandIndex; i++)
	{
		for (int j = 0; j < c_maxDealerHandIndex; j++)
		{
			int r = rand() % 3; // no split
			m_actions[i][j] = static_cast<Action>(r);
		}
	}

	for (int i=0; i < c_maxPlayerHandIndex; i++)
	{
		for (int j = 0; j < c_maxDealerHandIndex; j++)
		{
			m_actionCount[i][j] = 0;
		}
	}
}

void ActionTable::AdjustRandomActionTable(int entriesToAdjust)
{
	for (int e = 0; e < entriesToAdjust; e++)
	{
		int i = rand() % c_maxPlayerHandIndex;
		int j = rand() % c_maxDealerHandIndex;

		int r = 0;
		if (i < c_maxDealerHandIndex)
			r = rand() % 4;
		else
			r = rand() % 3;

		m_actions[i][j] = static_cast<Action>(r);
	}

	for (int i=0; i < c_maxPlayerHandIndex; i++)
	{
		for (int j = 0; j < c_maxDealerHandIndex; j++)
		{
			m_actionCount[i][j] = 0;
		}
	}
}


void ActionTable::LoadActionTable(const std::string & strActions)
{
	int index=0;

	for (int i=0; i < c_maxPlayerHandIndex; i++)
	{
		for (int j = 0; j < c_maxDealerHandIndex; j++)
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
	strActions.reserve(c_maxPlayerHandIndex*c_maxDealerHandIndex);

	for (int i=0; i < c_maxPlayerHandIndex; i++)
	{
		for (int j = 0; j < c_maxDealerHandIndex; j++)
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
	for (int i=0; i < c_maxPlayerHandIndex; i++)
	{
		for (int j = 0; j < c_maxDealerHandIndex; j++)
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
#endif

int MapPlayerHandToActionIndex(const PlayerSubHand & hand)
{
	// 0: 8 or less
	// 1 - 12: 9 through 20
	// 13 - 20: Soft 13 through 20
	// 21 - 30: Double A through 10

	const int handValue = hand.Value();
	assert(handValue != 21);

	if (hand.CanSplit() && hand.GetCard(0).Face() == CardFace::Ace)
		return 21;
	else if (hand.CanSplit())
		return 20 + hand.GetCard(0).Value();
	else if (hand.IsSoft() && handValue >= 13)
		return handValue;
	else if (handValue <= 8)
		return 0; //compress uninteresting values
	else
		return handValue - 8;
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

#if 0
int s_Results[5] = {0,0,0,0,0};

std::string GetNextAction(DealerHand & dealer, PlayerHand & player)
{
	return "s";
}

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
#endif 

bool CanDoAction(const PlayerSubHand& hand, Action action)
{
	switch (action)
	{
		case Action::Hit:
			return hand.CanHit();
		case Action::Stand:
			return true;
		case Action::DoubleDown:
			return hand.CanDoubleDown();
		case Action::Split:
			return hand.CanSplit();
		default:
			throw std::runtime_error("Unexpected action");
	}
}

void DoAction(PlayerHand& playerHand, PlayerSubHand& subHand, Action action, DeckShoeView& shoe)
{
	switch (action)
	{
		case Action::Hit:
		{
			const Card card = shoe.DealCard();
			subHand.AddCard(card);

			DebugOut(output << "Hit. " << card.ToString() << "(" << subHand.Value() << "), ");
			break;
		}
		case Action::Stand:
			DebugOut(output << "Stand.");
			break;
		case Action::DoubleDown:
		{
			const Card card = shoe.DealCard();
			subHand.DoubleDown(card);
			DebugOut(output << "Double Down. " << card.ToString() << "(" << subHand.Value() << "), ");
			break;
		}
		case Action::Split:
			DebugOut(output << "Split.");
			playerHand.Split(subHand, shoe);
			break;
		default:
			throw std::runtime_error("Unexpected action");
	}
}

struct ResultData
{
	double result = 0.0;
	int count = 0;
};

class ResultsCell
{
public:
	const ResultData& GetResultData(Action action) const;
	void AddResult(Action action, double result);
private:
	ResultData m_actionResults[4];
};

const ResultData& ResultsCell::GetResultData(Action action) const
{
	return m_actionResults[static_cast<int>(action)];
}

void ResultsCell::AddResult(Action action, double result)
{
	m_actionResults[static_cast<int>(action)].count++;
	m_actionResults[static_cast<int>(action)].result += result;
}

class ResultsTable
{
public:
	const ResultsCell& GetCell(int dealerHandIndex, int playerHandIndex) const;
	void RecordResult(int dealerHandIndex, int playerHandIndex, Action action, double result);

private:
	ResultsCell m_results[c_maxPlayerHandIndex][c_maxDealerHandIndex];
};

const ResultsCell& ResultsTable::GetCell(int dealerHandIndex, int playerHandIndex) const
{
	return m_results[playerHandIndex][dealerHandIndex];
}

void ResultsTable::RecordResult(int dealerHandIndex, int playerHandIndex, Action action, double result)
{
	m_results[playerHandIndex][dealerHandIndex].AddResult(action, result);
}

Action GetOptimalAction(const ResultsTable& resultTable, int dealerHandIndex, const PlayerSubHand& playerHand)
{
	const int playerHandIndex = MapPlayerHandToActionIndex(playerHand);
	const ResultsCell& cell = resultTable.GetCell(dealerHandIndex, playerHandIndex);

	std::array<Action, 4> allActions { Action::Stand, Action::Hit, Action::DoubleDown, Action::Split};
	Action optimalAction = Action::Stand;
	double optimalResult = std::numeric_limits<double>::min();

	for (Action action : allActions)
	{
		const ResultData& data = cell.GetResultData(action);
		if (data.count == 0 || !CanDoAction(playerHand, action))
			continue;

		const double adjustedResult = data.result / data.count;

		if (adjustedResult > optimalResult)
		{
			optimalResult = adjustedResult;
			optimalAction = action;
		}
	}

	return optimalAction;
}

double CompleteOptimally(DealerHand& dealerHand, PlayerHand& hand, const ResultsTable& resultTable, DeckShoeView& shoe, Action lastAction)
{
	double result = 0.0;
	auto& subHands = hand.SubHands();
	const int dealerHandIndex = MapDealerHandToActionIndex(dealerHand.Showing());

	if (lastAction != Action::Stand)
	{
		for (auto& subHand : subHands)
		{
			while (subHand.CanHit())
			{
				Action optimalAction;

				do {
					optimalAction = GetOptimalAction(resultTable, dealerHandIndex, subHand);
				} while (!CanDoAction(subHand, optimalAction));

				DoAction(hand, subHand, optimalAction, shoe);

				if (optimalAction == Action::Stand)
					break;
			}
		}
	}

	dealerHand.FlipHiddenCard();
	while (dealerHand.Value() < 17 || (dealerHand.Value() == 17 && dealerHand.IsSoft()))
	{
		auto card = shoe.DealCard();
		dealerHand.AddCard(card);
	}
	DebugOut(output << "\nDealer Final Hand: " << dealerHand.ToString() << " (" << dealerHand.Value() << ")" << std::endl);

	for (auto& subHand : subHands)
	{
		const double outcome = GetHandOutcome(subHand, dealerHand);
		result += subHand.Bet() * outcome;
	}

	return result;
}

void PrintResultsTable(const ResultsTable& results)
{
	std::array<Action, 4> allActions { Action::Stand, Action::Hit, Action::DoubleDown, Action::Split};

	for (int i=0; i < c_maxPlayerHandIndex; i++)
	{
		for (Action a : allActions)
		{
			for (int j = 0; j < c_maxDealerHandIndex; j++)
			{
				const ResultsCell& cell = results.GetCell(j, i);
				const ResultData& data = cell.GetResultData(a);

				if (data.count == 0)
					std::cout << "";
				else
					std::cout << data.result / data.count;

				std::cout << "\t";
			}
			std::cout << "\n";
		}
	}
}

int DoMarkovMonte(int iterations)
{
	ResultsTable resultsTable;

	srand(static_cast<unsigned int>(time(NULL)));
	DeckShoe shoeCards(6);
	MasterDeckShoeView shoe(shoeCards);
	Player dealer(std::string("Dealer"), 0);
	
	Player player("Player 1", 0.0);

	for (int round = 0; round != iterations; round++)
	{
		player.ClearStats();

		DealerHand dealerHand;
		PlayerHand playerHand(player);

		shoe.ReloadIfNecessary();

		playerHand.AddCard(shoe.DealCard());
		dealerHand.AddCard(shoe.DealCard());

		playerHand.AddCard(shoe.DealCard());
		dealerHand.AddCard(shoe.DealCard());

		// Check blackjack push
		// Check dealer blackjack lose
		// Check player blackjack win
		// Do decision tree

		PlayerSubHand& hand = playerHand.PrimaryHand();

		DebugOut(output << "Dealer showing: " << dealerHand.ToString() << " (" << dealerHand.Showing() << ")" << std::endl);
		DebugOut(output << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl);

		if (hand.IsBlackjack() && dealerHand.IsBlackjack())
		{
			// push
			DebugOut(output << "Dealer & Player Blackjack, push\n");
			continue;
		}
		else if (dealerHand.IsBlackjack())
		{
			DebugOut(output << "Dealer & Player Blackjack, push\n");
			// TODO: accumulate money
			continue;
		}
		else if (hand.IsBlackjack())
		{
			DebugOut(output << "Blackjack!\n");
			// TODO: accumulate money
			continue;
		}

		int dealerHandIndex = MapDealerHandToActionIndex(dealerHand.Showing());
		int playerHandIndex = MapPlayerHandToActionIndex(hand);
		
		int maxShoeOffset = 0;
		std::array<Action, 4> allActions { Action::Stand, Action::Hit, Action::DoubleDown, Action::Split};
		for (Action action : allActions)
		{
			if (!CanDoAction(hand, action))
				continue;

			if (action == Action::Split)
				assert(playerHandIndex > 20);

			PlayerHand handClone = playerHand;
			DealerHand dealerHandClone = dealerHand;
			DeckShoeView shoeClone = shoe;

			DebugOut(output << "\nTrying action: ");
			DoAction(handClone, handClone.PrimaryHand(), action, shoeClone);

			double result = CompleteOptimally(dealerHandClone, handClone, resultsTable, shoeClone, action);

			DebugOut(output << "Result: " << result << "\n");

			resultsTable.RecordResult(dealerHandIndex, playerHandIndex, action, result);

			maxShoeOffset = std::max(maxShoeOffset, shoeClone.Offset());
		}

		shoe.SetOffset(maxShoeOffset);

		player.SignalNewHand();

		//std::string temp;
		//std::getline(std::cin, temp);
	}

	PrintResultsTable(resultsTable);

	// Print out results table

	return 0;
}

#if 0
int DoMonte()
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
#endif

int main(int argc, char* argv[])
{
	int iterations = 1'000'000;
	if (argc >= 2)
		iterations = atoi(argv[1]);

	return DoMarkovMonte(iterations);
}
