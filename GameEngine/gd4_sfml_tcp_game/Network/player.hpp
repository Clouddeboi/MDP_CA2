#pragma once
class Player
{
public:
	Player();
	uint32_t GetHealth() const;
	uint32_t GetAmmo() const;
	void Write(OutputMemoryStream& out_stream) const;
	void Read(InputMemoryStream& in_stream);
	void Write(OutputMemoryBitStream& out_stream) const;
	void ReadBits(InputMemoryBitStream& in_stream);
	void ToString() const;
	


private:
	uint32_t health_;
	uint32_t ammo_;
	char name_[128];
	Vector3 position_;
	Quaternion rotation_;
	vector<int> weapons_;
	
};

