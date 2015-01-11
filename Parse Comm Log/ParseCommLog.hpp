#pragma once

#include <array>
#include <boost/chrono.hpp>
#include "ConsoleColor.h"
#include <iomanip>  
#include <iostream>
#include <vector>

typedef unsigned char BYTE;

// Find messages in stream of bytes
//Direction current_direction = { Direction::UNKNOWN };
//Direction last_direction = { Direction::UNKNOWN };


// Message position information - these are used to pull messages from stream of bytes
// These are NOT used to parse the contents of the messages
enum Direction
{
	UNKNOWN,
	RX,
	TX,
	COMMENT
};

enum class ParseState
{
	TX_POSITION_UNKNOWN,
	TX_START_OF_MESSAGE,
	TX_END_OF_MESSAGE,

	RX_POSITION_UNKNOWN,
	RX_START_OF_MESSAGE,
	RX_END_OF_MESSAGE,

	UNKNOWN,
};

enum class ExpectedResponse
{
	NO_RESPONSE_EXPECTED,
	GENERAL_POLL_RESPONSE_EXPECTED,
	LONG_POLL_RESPONSE_EXPECTED
};

class Status
{
public:
	Status(unsigned char status_byte)
		: raw_status(status_byte)
	{
		// Bit 0 is a flag indicating RX/TX
		direction_rx = (status_byte & RX) ? true : false;

		if (direction_rx)
		{
			// Only bytes RX'd have the status set to non-zero value
			overrun_error = (status_byte & OVERRUN) ? true : false;
			parity_error = (status_byte & PARITY) ? true : false;
			framing_error = (status_byte & FRAMING) ? true : false;
			break_error = (status_byte & BREAK) ? true : false;
		}
		else
		{
			// TX. Only bytes RX'd have the statuses
			overrun_error = false;
			parity_error = false;
			framing_error = false;
			break_error = false;
		}

		// Bits .1-.3 form a number
		unsigned char bits_321 = (status_byte & BITS_321_MASK) >> 1;
		comment_byte = (bits_321 == COMMENT);
	}

	bool rx() { return direction_rx; }

	bool addressByte() { return parity_error; }

	bool commentByte() { return comment_byte; }

	// Format the data for output
	friend std::ostream & operator << (std::ostream &o, Status &status)
	{
#ifdef __RAW_FORMAT__
		// raw format
		o << ' ' 
			<< std::setiosflags(std::ios::uppercase) 
			<< std::setfill('0') 
			<< std::setw(2) 
			<< std::hex 
			<< (int)status.raw_status;
#else
		// Formatted {B-break, F-framing, O-overrun, P-parity}
		if (status.parity_error)
		{
			o << green; //  << 'p';
		}
		if (status.break_error)
		{
			//	errors = true;
			o << red; // << 'b';
		}
		if (status.framing_error)
		{
			//errors = true;
			o << red; //  << 'f';
		}
		if (status.overrun_error)
		{
			//errors = true;
			o << red; // << 'o';
		}
#endif

		return o;
	}

	unsigned char raw_status;
	bool direction_rx;
	bool comment_byte;
	bool overrun_error;
	bool parity_error;
	bool framing_error;
	bool break_error;

private:
	// Bit .0 is used to flag an RX (or TX)
	static const unsigned char UNKNOWN = { 0x00 };
	static const unsigned char RX = { 0x01 };

	// Bits .3.2.1 form a number (Shift it right 1 to make it 0-7)
	static const unsigned char BITS_321_MASK = { 0x0E };
	static const unsigned char COMMENT = { 1 }; // Assuming the bits are shifted right once
	static const unsigned char RESERVE_2 = { 2 };
	static const unsigned char RESERVE_3 = { 3 };
	static const unsigned char RESERVE_4 = { 4 };
	static const unsigned char RESERVE_5 = { 5 };
	static const unsigned char RESERVE_6 = { 6 };
	static const unsigned char RESERVE_7 = { 7 };

	// Bits .7.6.5.4 come from the QUART status register "as-is"
	static const unsigned char OVERRUN = { 0x10 };
	static const unsigned char PARITY = { 0x20 };
	static const unsigned char FRAMING = { 0x40 };
	static const unsigned char BREAK = { 0x80 };
};

class StatusAndData
{
public:
	StatusAndData(unsigned char status, unsigned char data)
		:status(status),
		data(data)
	{
	}

	bool rx() { return (status.rx() && (!status.commentByte())); }

	bool tx() { return (!(status.rx() || status.commentByte())); }

	//warning this only works for SAS and other wkaeup protocols - add a bit for SOM and EOM in serialPort class when saving file
	bool addressByte() { return status.addressByte(); }

	bool commentByte() { return status.commentByte(); }

	bool broadcastPoll() { return data == 0x80; }

	bool generalPoll() { return data >= 0x81; }

	bool longPoll() { return data < 0x80; }

	// Format the data for output
	friend std::ostream & operator << (std::ostream &o, StatusAndData &status_and_data)
	{
		o << status_and_data.status
			<< std::setiosflags(std::ios::uppercase)
			<< std::setfill('0')
			<< std::setw(2)
			<< std::hex
			<< (int)status_and_data.data;
		return o;
	}

	Status status;
	unsigned char data;
};

bool START_OF_MESSAGE_DETECTED = { true };
bool NO_START_OF_MESSAGE_DETECTED = { false };
class Message
{
public:
	void Message::startNew(Direction _direction, bool _start_of_message_detected)
	{
		direction = _direction;

		start_of_message_detected = _start_of_message_detected;

		raw_status_and_data_bytes.clear();
	}

	Direction getDirection()
	{
		return direction;
	}

	// Format the data for output
	friend std::ostream & operator << (std::ostream &o, Message &message)
	{
#ifdef __INCLUDE__
		o << message.description.c_str() << ':' << message.data.c_str();
#else
		bool ascii = false;
		switch(message.direction)
		{
		case Direction::RX:
			o << "RX: ";
			break;
		case Direction::TX:
			o << "TX: ";
			break;
		case Direction::COMMENT:
			o << "//";
			ascii = true;
			break;
		default:
			o << "Direction UNKNOWN: ";
			break;
		}

		o << message.description;

		for (auto iter : message.raw_status_and_data_bytes)
		{
			if (ascii)
			{
				o << (unsigned char) iter.data;

			}
			else
			{
				o << ' ' << iter << white;
			}
		}
#endif
		return o;
	}

	Direction direction = { Direction::UNKNOWN };
	bool start_of_message_detected = { false };
	std::string description;
	std::vector<StatusAndData> raw_status_and_data_bytes;
};

unsigned char POLL_MASK = 0x80;

std::array <std::string, 0x100> long_poll =
{
	"LP 00 - ", // LP 00
	"LP 01 - SHUTDOWN",
	"LP 02 - STARTUP",
	"LP 03 - SOUND OFF",
	"LP 04 - SOUND ON",
	"LP 05 - REEL SPIN OR GAME PLAY SOUNDS DISABLED",
	"LP 06 - ENABLE BILL ACCEPTOR",
	"LP 07 - DISABLE BILL ACCEPTOR",
	"LP 08 - CONFIGURE BILL DENOMINATIONS",
	"LP 09 - ENABLE/DISABLE GAME N",
	"LP 0A - ENTER MAINTENANCE MODE",
	"LP 0B - EXIT MAINTENANCE MODE",
	"LP 0C",
	"LP 0D",
	"LP 0E - ENABLE/DISABLE REAL TIME EVENT REPORTING",
	"LP 0F - SEND METERS 10-15",

	"LP 10 - SEND TOTAL CANCELLED CREDITS METER", // LP 10
	"LP 11 - SEND TOTAL COIN IN METER",
	"LP 12 - SEND TOTAL COIN OUT METER",
	"LP 13 - SEND TOTAL DROP METER",
	"LP 14 - SEND TOTAL JACKPOT METER",
	"LP 15 - SEND GAMES PLAYED METER",
	"LP 16 - SEND GAMES WON METER",
	"LP 17 - SEND GAMES LOST METER",
	"LP 18 - SEND GAMES PLAYER SINCE LAST POWER UP",
	"LP 19 - SEND METERS 11 - 15",
	"LP 1A - SEND CURRENT CREDITS",
	"LP 1B - SEND HANDPAY INFORMATION",
	"LP 1C - SEND METERS",
	"LP 1D",
	"LP 1E - SEND TOTAL BILL METERS (# OF BILLS)",
	"LP 1F - SEND GAMING MACHINE ID & INFORMATION",

	"LP 20 - SEND TOTAL BILL METERS (VALUE OF BILLS)", // LP 20
	"LP 21 - ROM SIGNATURE VERIFICATION",
	"LP 22",
	"LP 23",
	"LP 24",
	"LP 25",
	"LP 26",
	"LP 27",
	"LP 28",
	"LP 29",
	"LP 2A - SEND TRUE COIN IN",
	"LP 2B - SEND TRUE COIN OUT",
	"LP 2C - SEND CURRENT HOPPER LEVEL",
	"LP 2D - SEND TOTAL HAND PAID CANCELLED CREDITS",
	"LP 2E - DELAY GAME",
	"LP 2F - SEND SELECTED METERS FOR GAME N",

	"LP 30", // LP 30
	"LP 31 - SEND $1 BILLS IN METER",
	"LP 32 - SEND $2 BILLS IN METER",
	"LP 33 - SEND $5 BILLS IN METER",
	"LP 34 - SEND $10 BILLS IN METER",
	"LP 35 - SEND $20 BILLS IN METER",
	"LP 36 - SEND $50 BILLS IN METER",
	"LP 37 - SEND $100 BILLS IN METER",
	"LP 38 - SEND $500 BILLS IN METER",
	"LP 39 - SEND $1,000 BILLS IN METER",
	"LP 3A - SEND $200 BILLS IN METER",
	"LP 3B - SEND $25 BILLS IN METER",
	"LP 3C - SEND $2,000 BILLS IN METER",
	"LP 3D - SEND CASH OUT TICKET INFORMATION",
	"LP 3E - SEND $2,500 BILLS IN METER",
	"LP 3F - SEND $5,000 BILLS IN METER",

	"LP 40 - SEND $10,000 BILLS IN METER", // LP 40
	"LP 41 - SEND $20,000 BILLS IN METER",
	"LP 42 - SEND $25,000 BILLS IN METER",
	"LP 43 - SEND $50,000 BILLS IN METER",
	"LP 44 - SEND $100,000 BILLS IN METER",
	"LP 45 - SEND $250 BILLS IN METER",
	"LP 46 - SEND CREDIT AMOUNT OF ALL BILLS ACCEPTED",
	"LP 47 - SEND COIN AMOUNT ACCEPTED FROM AN EXTERNAL COIN ACCEPTOR",
	"LP 48 - SEND LAST BILL ACCEPTED INFORMATION",
	"LP 49 - SEND NUMBER OF BILLS CURRENTLY IN STACKER",
	"LP 4A - SEND TOTAL CREDIT AMOUNT OF ALL BILLS CURRENTLY IN STACKED",
	"LP 4B",
	"LP 4C - SET SECURE ENHANCED VALIDATION ID",
	"LP 4D - SEND ENHANCED VALIDATION INFORMATION",
	"LP 4E",
	"LP 4F - SEND CURRENT HOPPER STATUS",

	"LP 50 - SEND VALIDATION METERS", // LP 50
	"LP 51 - SEND TOTAL NUMBER OF GAMES IMPLEMENTED",
	"LP 52 - SEND GAME N METERS",
	"LP 53 - SEND GAME N CONFIGURATION",
	"LP 54 - SEND SAS VERSION ID AND EGM SERIAL #",
	"LP 55 - SEND SELECTED GAME NUMBER",
	"LP 56 - SEND ENABLED GAME NUMBERS",
	"LP 57 - SEND PENDING CASHOUT INFORMATION",
	"LP 58 - RECEIVE VALIDATION NUMBER",
	"LP 59",
	"LP 5A",
	"LP 5B",
	"LP 5C",
	"LP 5D",
	"LP 5E",
	"LP 5F",

	"LP 60", // LP 60
	"LP 61",
	"LP 62",
	"LP 63",
	"LP 64",
	"LP 65",
	"LP 66",
	"LP 67",
	"LP 68",
	"LP 69",
	"LP 6A",
	"LP 6B",
	"LP 6C",
	"LP 6D",
	"LP 6E - SEND AUTHENTICATION INFORMATION",
	"LP 6F - SEND EXTENDED METERS FOR GAME N",

	"LP 70 - SEND TICKET VALIDATION DATA", // LP 70
	"LP 71 - REDEEM TICKET",
	"LP 72 - AFT TRANSFER FUNDS",
	"LP 73 - AFT REGISTER GAMING MACHINE",
	"LP 74 - ADT GAME LOCK AND STATUS REQUEST",
	"LP 75 - SET AFT RECEIPT DATA",
	"LP 76 - SET CUSTOM AFT TICKET DATA",
	"LP 77",
	"LP 78",
	"LP 79",
	"LP 7A",
	"LP 7B - EXTENDED VALIDATION STATUS",
	"LP 7C - SET EXTENDED TICKET DATA",
	"LP 7D - SET TICKET DATA",
	"LP 7E - SEND CURRENT DATE TIME",
	"LP 7F - SET CURRENT DATE TIME",

	"LP 80 - RECEIVE PROGRESSIVE INFORMATION", // LP 80
	"LP 81",
	"LP 82",
	"LP 83 - SEND CUMULATIVE PROGRESSIVE WINS",
	"LP 84 - SEND PROGRESSIVE WIN AMOUNT",
	"LP 85 - SEND SAS PROGRESSIVE WIN AMOUNT",
	"LP 86 - RECEIVE MULTIPLE PROGRESSIVE LEVELS",
	"LP 87 - SEND MULTIPLE SAS PROGRESSIVE WIN AMOUNTS",
	"LP 88",
	"LP 89",
	"LP 8A - INITIATE A LEGACY BONUS PAY",
	"LP 8B - INITIATE MULTIPLIED JACKPOT MODE (OBSOLETE)",
	"LP 8C - ENTER/EXIT TOURNAMENT MODE",
	"LP 8D",
	"LP 8E - SEND CARD INFORMATION",
	"LP 8F - SEND PHYSICAL REEL STOP INFORMATION",

	"LP 90 - SEND LEGACY BONUS WIN AMOUNT", // LP 90
	"LP 91",
	"LP 92",
	"LP 93",
	"LP 94 - REMOTE HANDPAY RESET",
	"LP 95 - SEND TOURNAMENT GAMES PLAYED",
	"LP 96 - SEND TOURNAMENT GAMES WON",
	"LP 97 - SEND TOURNAMENT GAMES WAGERED",
	"LP 98 - SEND TOURNAMENT CREDITS WAGERED",
	"LP 99 - SEND METERS 95-98",
	"LP 9A - SEND LEGACY BONUS METERS",
	"LP 9B",
	"LP 9C",
	"LP 9D",
	"LP 9E",
	"LP 9F",

	"LP A0 - SEND ENABLED FEATURES", // LP A0
	"LP A1",
	"LP A2",
	"LP A3",
	"LP A4 - SEND CASH OUT LIMIT",
	"LP A5",
	"LP A6",
	"LP A7",
	"LP A8 - ENABLED JACPOT HANDPAY RESET METHOD",
	"LP A9",
	"LP AA - ENABLE/DISABLE AUTO REBET",
	"LP AB",
	"LP AC",
	"LP AD",
	"LP AE",
	"LP AF",

	"LP B0 - MULTI-DENOM PREAMBLE", // LP B0
	"LP B1 - SEND CURRENT PLAYER DENOMINATION",
	"LP B2 - SEND ENABLED PLAYER DENOMINATIONS",
	"LP B3 - SEND TOKEN DENOMINATION",
	"LP B4 - SEND WAGER CATEGORY INFORMATION",
	"LP B5 - SEND EXTENDED GAME N INFORMATION"
	"LP B6",
	"LP B7",
	"LP B8",
	"LP B9",
	"LP BA",
	"LP BB",
	"LP BC",
	"LP BD",
	"LP BE",
	"LP BF",

	"LP C0", // LP C0
	"LP C1",
	"LP C2",
	"LP C3",
	"LP C4",
	"LP C5",
	"LP C6",
	"LP C7",
	"LP C8",
	"LP C9",
	"LP CA",
	"LP CB",
	"LP CC",
	"LP CD",
	"LP CE",
	"LP CF",

	"LP D0", // LP D0
	"LP D1",
	"LP D2",
	"LP D3",
	"LP D4",
	"LP D5",
	"LP D6",
	"LP D7",
	"LP D8",
	"LP D9",
	"LP DA",
	"LP DB",
	"LP DC",
	"LP DD",
	"LP DE",
	"LP DF",

	"LP E0", // LP E0
	"LP E1",
	"LP E2",
	"LP E3",
	"LP E4",
	"LP E5",
	"LP E6",
	"LP E7",
	"LP E8",
	"LP E9",
	"LP EA",
	"LP EB",
	"LP EC",
	"LP ED",
	"LP EE",
	"LP EF",

	"LP F0", // LP F0
	"LP F1",
	"LP F2",
	"LP F3",
	"LP F4",
	"LP F5",
	"LP F6",
	"LP F7",
	"LP F8",
	"LP F9",
	"LP FA",
	"LP FB",
	"LP FC",
	"LP FD",
	"LP FE",
	"LP FF - EVENT RESPONSE TO LONG POLL"
};

std::array <std::string, 0x100> long_poll_response =
{
	"LP 00 - ",
	"LP 01 - ",
	"LP 02 - ",
	"LP 03 - ",
	"LP 04 - ",
	"LP 05 - ",
	"LP 06 - ",
	"LP 07 - ",
	"LP 08 - ",
	"LP 09 - ",
	"LP 0A - ",
	"LP 0B - ",
	"LP 0C - ",
	"LP 0D - ",
	"LP 0E - ",
	"LP 0F - ",

	"LP 10 - ",
	"LP 11 - ",
	"LP 12 - ",
	"LP 13 - ",
	"LP 14 - ",
	"LP 15 - ",
	"LP 16 - ",
	"LP 17 - ",
	"LP 18 - ",
	"LP 19 - ",
	"LP 1A - ",
	"LP 1B - ",
	"LP 1C - ",
	"LP 1D - ",
	"LP 1E - ",
	"LP 1F - ",

	"LP 20 - ",
	"LP 21 - ",
	"LP 22 - ",
	"LP 23 - ",
	"LP 24 - ",
	"LP 25 - ",
	"LP 26 - ",
	"LP 27 - ",
	"LP 28 - ",
	"LP 29 - ",
	"LP 2A - ",
	"LP 2B - ",
	"LP 2C - ",
	"LP 2D - ",
	"LP 2E - ",
	"LP 2F - ",

	"LP 30 - ",
	"LP 31 - ",
	"LP 32 - ",
	"LP 33 - ",
	"LP 34 - ",
	"LP 35 - ",
	"LP 36 - ",
	"LP 37 - ",
	"LP 38 - ",
	"LP 39 - ",
	"LP 3A - ",
	"LP 3B - ",
	"LP 3C - ",
	"LP 3D - ",
	"LP 3E - ",
	"LP 3F - ",

	"LP 40 - ",
	"LP 41 - ",
	"LP 42 - ",
	"LP 43 - ",
	"LP 44 - ",
	"LP 45 - ",
	"LP 46 - ",
	"LP 47 - ",
	"LP 48 - ",
	"LP 49 - ",
	"LP 4A - ",
	"LP 4B - ",
	"LP 4C - ",
	"LP 4D - ",
	"LP 4E - ",
	"LP 4F - ",

	"LP 50 - ",
	"LP 51 - ",
	"LP 52 - ",
	"LP 53 - ",
	"LP 54 - ",
	"LP 55 - ",
	"LP 56 - ",
	"LP 57 - ",
	"LP 58 - ",
	"LP 59 - ",
	"LP 5A - ",
	"LP 5B - ",
	"LP 5C - ",
	"LP 5D - ",
	"LP 5E - ",
	"LP 5F - ",

	"LP 60 - ",
	"LP 61 - ",
	"LP 62 - ",
	"LP 63 - ",
	"LP 64 - ",
	"LP 65 - ",
	"LP 66 - ",
	"LP 67 - ",
	"LP 68 - ",
	"LP 69 - ",
	"LP 6A - ",
	"LP 6B - ",
	"LP 6C - ",
	"LP 6D - ",
	"LP 6E - ",
	"LP 6F - ",

	"LP 70 - ",
	"LP 71 - ",
	"LP 72 - ",
	"LP 73 - ",
	"LP 74 - ",
	"LP 75 - ",
	"LP 76 - ",
	"LP 77 - ",
	"LP 78 - ",
	"LP 79 - ",
	"LP 7A - ",
	"LP 7B - ",
	"LP 7C - ",
	"LP 7D - ",
	"LP 7E - ",
	"LP 7F - ",

	"LP 80 - ",
	"LP 81 - ",
	"LP 82 - ",
	"LP 83 - ",
	"LP 84 - ",
	"LP 85 - ",
	"LP 86 - ",
	"LP 87 - ",
	"LP 88 - ",
	"LP 89 - ",
	"LP 8A - ",
	"LP 8B - ",
	"LP 8C - ",
	"LP 8D - ",
	"LP 8E - ",
	"LP 8F - ",

	"LP 90 - ",
	"LP 91 - ",
	"LP 92 - ",
	"LP 93 - ",
	"LP 94 - ",
	"LP 95 - ",
	"LP 96 - ",
	"LP 97 - ",
	"LP 98 - ",
	"LP 99 - ",
	"LP 9A - ",
	"LP 9B - ",
	"LP 9C - ",
	"LP 9D - ",
	"LP 9E - ",
	"LP 9F - ",

	"LP A0 - ",
	"LP A1 - ",
	"LP A2 - ",
	"LP A3 - ",
	"LP A4 - ",
	"LP A5 - ",
	"LP A6 - ",
	"LP A7 - ",
	"LP A8 - ",
	"LP A9 - ",
	"LP AA - ",
	"LP AB - ",
	"LP AC - ",
	"LP AD - ",
	"LP AE - ",
	"LP AF - ",

	"LP B0 - ",
	"LP B1 - ",
	"LP B2 - ",
	"LP B3 - ",
	"LP B4 - ",
	"LP B5 - ",
	"LP B6 - ",
	"LP B7 - ",
	"LP B8 - ",
	"LP B9 - ",
	"LP BA - ",
	"LP BB - ",
	"LP BC - ",
	"LP BD - ",
	"LP BE - ",
	"LP BF - ",

	"LP C0 - ",
	"LP C1 - ",
	"LP C2 - ",
	"LP C3 - ",
	"LP C4 - ",
	"LP C5 - ",
	"LP C6 - ",
	"LP C7 - ",
	"LP C8 - ",
	"LP C9 - ",
	"LP CA - ",
	"LP CB - ",
	"LP CC - ",
	"LP CD - ",
	"LP CE - ",
	"LP CF - ",

	"LP D0 - ",
	"LP D1 - ",
	"LP D2 - ",
	"LP D3 - ",
	"LP D4 - ",
	"LP D5 - ",
	"LP D6 - ",
	"LP D7 - ",
	"LP D8 - ",
	"LP D9 - ",
	"LP DA - ",
	"LP DB - ",
	"LP DC - ",
	"LP DD - ",
	"LP DE - ",
	"LP DF - ",

	"LP E0 - ",
	"LP E1 - ",
	"LP E2 - ",
	"LP E3 - ",
	"LP E4 - ",
	"LP E5 - ",
	"LP E6 - ",
	"LP E7 - ",
	"LP E8 - ",
	"LP E9 - ",
	"LP EA - ",
	"LP EB - ",
	"LP EC - ",
	"LP ED - ",
	"LP EE - ",
	"LP EF - ",

	"LP F0 - ",
	"LP F1 - ",
	"LP F2 - ",
	"LP F3 - ",
	"LP F4 - ",
	"LP F5 - ",
	"LP F6 - ",
	"LP F7 - ",
	"LP F8 - ",
	"LP F9 - ",
	"LP FA - ",
	"LP FB - ",
	"LP FC - ",
	"LP FD - ",
	"LP FE - ",
	"LP FF - "
};

std::array <std::string, 0x100> exceptions =
{
	"EXCEPTION 00 - ",
	"EXCEPTION 01 - ",
	"EXCEPTION 02 - ",
	"EXCEPTION 03 - ",
	"EXCEPTION 04 - ",
	"EXCEPTION 05 - ",
	"EXCEPTION 06 - ",
	"EXCEPTION 07 - ",
	"EXCEPTION 08 - ",
	"EXCEPTION 09 - ",
	"EXCEPTION 0A - ",
	"EXCEPTION 0B - ",
	"EXCEPTION 0C - ",
	"EXCEPTION 0D - ",
	"EXCEPTION 0E - ",
	"EXCEPTION 0F - ",

	"EXCEPTION 10 - ",
	"EXCEPTION 11 - SLOT DOOR WAS OPENED",
	"EXCEPTION 12 - SLOT DOOR WAS CLOSED",
	"EXCEPTION 13 - DROP DOOR WAS OPENED",
	"EXCEPTION 14 - DROP DOOR WAS CLOSED",
	"EXCEPTION 15 - ",
	"EXCEPTION 16 - ",
	"EXCEPTION 17 - ",
	"EXCEPTION 18 - ",
	"EXCEPTION 19 - ",
	"EXCEPTION 1A - ",
	"EXCEPTION 1B - ",
	"EXCEPTION 1C - ",
	"EXCEPTION 1D - ",
	"EXCEPTION 1E - ",
	"EXCEPTION 1F - ",

	"EXCEPTION 20 - ",
	"EXCEPTION 21 - ",
	"EXCEPTION 22 - ",
	"EXCEPTION 23 - ",
	"EXCEPTION 24 - ",
	"EXCEPTION 25 - ",
	"EXCEPTION 26 - ",
	"EXCEPTION 27 - ",
	"EXCEPTION 28 - ",
	"EXCEPTION 29 - ",
	"EXCEPTION 2A - ",
	"EXCEPTION 2B - ",
	"EXCEPTION 2C - ",
	"EXCEPTION 2D - ",
	"EXCEPTION 2E - ",
	"EXCEPTION 2F - ",

	"EXCEPTION 30 - ",
	"EXCEPTION 31 - ",
	"EXCEPTION 32 - ",
	"EXCEPTION 33 - ",
	"EXCEPTION 34 - ",
	"EXCEPTION 35 - ",
	"EXCEPTION 36 - ",
	"EXCEPTION 37 - ",
	"EXCEPTION 38 - ",
	"EXCEPTION 39 - ",
	"EXCEPTION 3A - ",
	"EXCEPTION 3B - ",
	"EXCEPTION 3C - ",
	"EXCEPTION 3D - ",
	"EXCEPTION 3E - ",
	"EXCEPTION 3F - ",

	"EXCEPTION 40 - ",
	"EXCEPTION 41 - ",
	"EXCEPTION 42 - ",
	"EXCEPTION 43 - ",
	"EXCEPTION 44 - ",
	"EXCEPTION 45 - ",
	"EXCEPTION 46 - ",
	"EXCEPTION 47 - ",
	"EXCEPTION 48 - ",
	"EXCEPTION 49 - ",
	"EXCEPTION 4A - ",
	"EXCEPTION 4B - ",
	"EXCEPTION 4C - ",
	"EXCEPTION 4D - ",
	"EXCEPTION 4E - ",
	"EXCEPTION 4F - ",

	"EXCEPTION 50 - ",
	"EXCEPTION 51 - ",
	"EXCEPTION 52 - ",
	"EXCEPTION 53 - ",
	"EXCEPTION 54 - ",
	"EXCEPTION 55 - ",
	"EXCEPTION 56 - ",
	"EXCEPTION 57 - ",
	"EXCEPTION 58 - ",
	"EXCEPTION 59 - ",
	"EXCEPTION 5A - ",
	"EXCEPTION 5B - ",
	"EXCEPTION 5C - ",
	"EXCEPTION 5D - ",
	"EXCEPTION 5E - ",
	"EXCEPTION 5F - ",

	"EXCEPTION 60 - ",
	"EXCEPTION 61 - ",
	"EXCEPTION 62 - ",
	"EXCEPTION 63 - ",
	"EXCEPTION 64 - ",
	"EXCEPTION 65 - ",
	"EXCEPTION 66 - ",
	"EXCEPTION 67 - ",
	"EXCEPTION 68 - ",
	"EXCEPTION 69 - ",
	"EXCEPTION 6A - ",
	"EXCEPTION 6B - ",
	"EXCEPTION 6C - ",
	"EXCEPTION 6D - ",
	"EXCEPTION 6E - ",
	"EXCEPTION 6F - ",

	"EXCEPTION 70 - ",
	"EXCEPTION 71 - ",
	"EXCEPTION 72 - ",
	"EXCEPTION 73 - ",
	"EXCEPTION 74 - ",
	"EXCEPTION 75 - ",
	"EXCEPTION 76 - ",
	"EXCEPTION 77 - ",
	"EXCEPTION 78 - ",
	"EXCEPTION 79 - ",
	"EXCEPTION 7A - ",
	"EXCEPTION 7B - ",
	"EXCEPTION 7C - ",
	"EXCEPTION 7D - ",
	"EXCEPTION 7E - ",
	"EXCEPTION 7F - ",

	"EXCEPTION 80 - ",
	"EXCEPTION 81 - ",
	"EXCEPTION 82 - ",
	"EXCEPTION 83 - ",
	"EXCEPTION 84 - ",
	"EXCEPTION 85 - ",
	"EXCEPTION 86 - ",
	"EXCEPTION 87 - ",
	"EXCEPTION 88 - ",
	"EXCEPTION 89 - ",
	"EXCEPTION 8A - ",
	"EXCEPTION 8B - ",
	"EXCEPTION 8C - ",
	"EXCEPTION 8D - ",
	"EXCEPTION 8E - ",
	"EXCEPTION 8F - ",

	"EXCEPTION 90 - ",
	"EXCEPTION 91 - ",
	"EXCEPTION 92 - ",
	"EXCEPTION 93 - ",
	"EXCEPTION 94 - ",
	"EXCEPTION 95 - ",
	"EXCEPTION 96 - ",
	"EXCEPTION 97 - ",
	"EXCEPTION 98 - ",
	"EXCEPTION 99 - ",
	"EXCEPTION 9A - ",
	"EXCEPTION 9B - ",
	"EXCEPTION 9C - ",
	"EXCEPTION 9D - ",
	"EXCEPTION 9E - ",
	"EXCEPTION 9F - ",

	"EXCEPTION A0 - ",
	"EXCEPTION A1 - ",
	"EXCEPTION A2 - ",
	"EXCEPTION A3 - ",
	"EXCEPTION A4 - ",
	"EXCEPTION A5 - ",
	"EXCEPTION A6 - ",
	"EXCEPTION A7 - ",
	"EXCEPTION A8 - ",
	"EXCEPTION A9 - ",
	"EXCEPTION AA - ",
	"EXCEPTION AB - ",
	"EXCEPTION AC - ",
	"EXCEPTION AD - ",
	"EXCEPTION AE - ",
	"EXCEPTION AF - ",

	"EXCEPTION B0 - ",
	"EXCEPTION B1 - ",
	"EXCEPTION B2 - ",
	"EXCEPTION B3 - ",
	"EXCEPTION B4 - ",
	"EXCEPTION B5 - ",
	"EXCEPTION B6 - ",
	"EXCEPTION B7 - ",
	"EXCEPTION B8 - ",
	"EXCEPTION B9 - ",
	"EXCEPTION BA - ",
	"EXCEPTION BB - ",
	"EXCEPTION BC - ",
	"EXCEPTION BD - ",
	"EXCEPTION BE - ",
	"EXCEPTION BF - ",

	"EXCEPTION C0 - ",
	"EXCEPTION C1 - ",
	"EXCEPTION C2 - ",
	"EXCEPTION C3 - ",
	"EXCEPTION C4 - ",
	"EXCEPTION C5 - ",
	"EXCEPTION C6 - ",
	"EXCEPTION C7 - ",
	"EXCEPTION C8 - ",
	"EXCEPTION C9 - ",
	"EXCEPTION CA - ",
	"EXCEPTION CB - ",
	"EXCEPTION CC - ",
	"EXCEPTION CD - ",
	"EXCEPTION CE - ",
	"EXCEPTION CF - ",

	"EXCEPTION D0 - ",
	"EXCEPTION D1 - ",
	"EXCEPTION D2 - ",
	"EXCEPTION D3 - ",
	"EXCEPTION D4 - ",
	"EXCEPTION D5 - ",
	"EXCEPTION D6 - ",
	"EXCEPTION D7 - ",
	"EXCEPTION D8 - ",
	"EXCEPTION D9 - ",
	"EXCEPTION DA - ",
	"EXCEPTION DB - ",
	"EXCEPTION DC - ",
	"EXCEPTION DD - ",
	"EXCEPTION DE - ",
	"EXCEPTION DF - ",

	"EXCEPTION E0 - ",
	"EXCEPTION E1 - ",
	"EXCEPTION E2 - ",
	"EXCEPTION E3 - ",
	"EXCEPTION E4 - ",
	"EXCEPTION E5 - ",
	"EXCEPTION E6 - ",
	"EXCEPTION E7 - ",
	"EXCEPTION E8 - ",
	"EXCEPTION E9 - ",
	"EXCEPTION EA - ",
	"EXCEPTION EB - ",
	"EXCEPTION EC - ",
	"EXCEPTION ED - ",
	"EXCEPTION EE - ",
	"EXCEPTION EF - ",

	"EXCEPTION F0 - ",
	"EXCEPTION F1 - ",
	"EXCEPTION F2 - ",
	"EXCEPTION F3 - ",
	"EXCEPTION F4 - ",
	"EXCEPTION F5 - ",
	"EXCEPTION F6 - ",
	"EXCEPTION F7 - ",
	"EXCEPTION F8 - ",
	"EXCEPTION F9 - ",
	"EXCEPTION FA - ",
	"EXCEPTION FB - ",
	"EXCEPTION FC - ",
	"EXCEPTION FD - ",
	"EXCEPTION FE - ",
	"EXCEPTION FF - "
};

Message current_message;
std::vector<Message> messages;
std::vector<Message>::size_type messages_index = { 0 };


enum LastRequest
{
	UNKNOWN_REQUEST,
	BP_REQUEST,
	GP_REQUEST,
	LP_REQUEST,
};

LastRequest last_request = { UNKNOWN_REQUEST };

