#include "SocketWrapperPCH.hpp"
#include "Player.hpp"

Player::Player()
    : health_(10)
    , ammo_(3)
    , name_("Default")
    , position_(10, 10, 10)
    , rotation_(0, 0, 0, 1)
    , weapons_{ 60, 70, 80, 90, 100 }
{}

uint32_t Player::GetHealth() const
{
    return health_;
}

uint32_t Player::GetAmmo() const
{
    return ammo_;
}

void Player::Write(OutputMemoryStream& out_stream) const
{
    out_stream.Write(health_);
    out_stream.Write(ammo_);
    out_stream.Write(name_, 128);
    out_stream.Write(position_);
    out_stream.Write(rotation_);
    out_stream.Write(weapons_);
}

void Player::Read(InputMemoryStream & in_stream)
{
    in_stream.Read(health_);
    in_stream.Read(ammo_);
    in_stream.Read(name_, 128);
    in_stream.Read(position_);
    in_stream.Read(rotation_);
    in_stream.Read(weapons_);
}

void Player::Write(OutputMemoryBitStream& out_bit_stream) const
{
    out_bit_stream.WriteBits(health_, 4);
    out_bit_stream.WriteBits(ammo_, 2);
    uint8_t name_length = static_cast<uint8_t>(strlen(name_));
    std::cout << "Write namelength " << static_cast<int>(name_length) << std::endl;
    out_bit_stream.WriteBits(name_length, 8);
    out_bit_stream.WriteBits(name_, (name_length * 8));
    //Position uses entropy encoding
    out_bit_stream.WritePos(position_);
    //Quaternion uses fixed point compression
    out_bit_stream.Write(rotation_);
    out_bit_stream.Write(weapons_);
}

void Player::ReadBits(InputMemoryBitStream & in_bit_stream)
{
    uint8_t name_length;
    in_bit_stream.ReadBits(&health_, 4);
    std::cout << "Health " << health_ << std::endl;
    in_bit_stream.ReadBits(&ammo_, 2);
    std::cout << "Health " << ammo_ << std::endl;
    in_bit_stream.ReadBits(&name_length, 8);
    std::cout << "Name Length " << static_cast<int>(name_length) << std::endl;
    in_bit_stream.ReadBits(&name_, ((static_cast<int>(name_length)) * 8));
    std::cout << "Name " << name_ << std::endl;
    in_bit_stream.ReadPos(position_);
    std::cout << "Position " << position_.mX << std::endl;
    in_bit_stream.Read(rotation_);
    std::cout << "Rotation " << rotation_.mX << std::endl;
    in_bit_stream.Read(weapons_);
}

void Player::ToString() const
{
    std::cout << name_ << " has health: " << health_ << ", Ammo: " << ammo_ << " Position: " << "(" << position_.mX << ", " << position_.mY << ", " << position_.mZ << ")" << "Quaternion: (" << rotation_.mX << ", " << rotation_.mY << ", " << rotation_.mZ << ", " << rotation_.mW << ")" << std::endl;
    for (const int weapon : weapons_)
    {
        std::cout << weapon << std::endl;
    }
 }
