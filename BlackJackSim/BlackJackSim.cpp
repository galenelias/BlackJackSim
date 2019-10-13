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


std::string Card::ToString() const
{
	constexpr char* g_szSuitNames[4] = { "S", "H", "C", "D" };
	constexpr char* g_szFaceNames[13] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };

	std::string strFaceName = g_szFaceNames[static_cast<size_t>(Face())];
	std::string strSuitName = g_szSuitNames[static_cast<size_t>(Suit())];

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

constexpr int c_maxPlayerHandIndex = 31;
constexpr int c_maxDealerHandIndex = 10;

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

std::string GetNextAction(DealerHand & dealer, PlayerHand & player)
{
	return "s";
}

bool RunOneRoundInteractively(MasterDeckShoeView & shoe, std::vector<Player> & players)
{
	DealerHand dealerHand;
	std::list<PlayerSubHand> playerHands;

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
		std::cout << "Dealer showing: " << dealerHand.ToString() << " (" << dealerHand.Showing() << ")" << std::endl;
		std::cout << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;

		while (hand.CanHit() && !dealerHand.IsBlackjack())
		{
			std::cout << "Action ((h)it, (s)tand, s(p)lit, (d)ouble down)? ";
			std::string userAction;
			std::cin >> userAction;

			Action action;
			if (userAction == "h") action = Action::Hit;
			else if (userAction == "s") action = Action::Stand;
			else if (userAction == "p") action = Action::Split;
			else if (userAction == "d") action = Action::DoubleDown;
			else continue;

			if ((action == Action::DoubleDown && !hand.CanDoubleDown()) || (action == Action::Split && !hand.CanSplit()))
			{
				std::cout << "Can't " << GetActionString(action) << " right now\n";
			}

			if (action == Action::Hit)
			{
				std::cout << "Hitting\n";
				hand.AddCard(shoe.DealCard());
				std::cout << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
			}
			else if (action == Action::Stand)
			{
				std::cout << "Standing\n";
				break;
			}
			else if (action == Action::Split && hand.CanSplit())
			{
				// TODO
				playerHands.emplace_back(hand.Split(shoe));
				std::cout << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
			}
			else if (action == Action::Split)
			{
				std::cout << "Hand not split-able" << std::endl;
			}
			else if (action == Action::DoubleDown)
			{
				const Card card = shoe.DealCard();
				hand.DoubleDown(card);
				std::cout << hand.PlayerName() <<  "'s hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
				break;
			}
		}
	}

	// TODO: check for all players busted
	dealerHand.FlipHiddenCard();

	std::cout << "Dealer: " << dealerHand.ToString() << " (" << dealerHand.Value() << ")" << std::endl;

	while (dealerHand.Value() < 17 || (dealerHand.Value() == 17 && dealerHand.IsSoft()))
	{
		auto card = shoe.DealCard();
		dealerHand.AddCard(card);
		output << "Dealer: " << dealerHand.ToString() << " (" << dealerHand.Value() << ")" << std::endl;
	}

	for (auto & player : players)
	{
		player.SignalNewHand();
	}

	for (auto & hand : playerHands)
	{
		std::cout << std::endl;
		std::cout << hand.PlayerName() <<  "'s final hand: " << hand.ToString() << " (" << hand.Value() << ")" << std::endl;
		const double outcome = GetHandOutcome(hand, dealerHand);

		hand.PayoutHand(outcome);
		std::cout << "Payout: " << hand.Bet() * outcome << " (" << hand.Owner().Money() << ")\n\n";
	}

	return true;
}

void PlayInteractively()
{
	DeckShoe shoeCards(6);
	MasterDeckShoeView shoe(shoeCards);
	Player dealer(std::string("Dealer"), 0);

	std::vector<Player> players;

	double bestExpectedValue = -100;

	players.emplace_back("Player 1", 0.0);
	//players.emplace_back("Player 2", 500.0);


	bool fRunMore = true;

	//while (fRunMore)
	for (;;)
	{
		players[0].ClearStats();

		fRunMore = RunOneRoundInteractively(shoe, players);
	}
}

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
	constexpr std::array<Action, 4> allActions = { Action::Stand, Action::Hit, Action::DoubleDown, Action::Split};

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
		constexpr std::array<Action, 4> allActions { Action::Stand, Action::Hit, Action::DoubleDown, Action::Split};
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

	return 0;
}


int main(int argc, char* argv[])
{
	int iterations = 1'000'000;
	if (argc >= 2)
		iterations = atoi(argv[1]);

	//PlayInteractively();
	return DoMarkovMonte(iterations);
}
