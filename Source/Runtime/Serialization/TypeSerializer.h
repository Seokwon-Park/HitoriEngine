#pragma once

inline void to_json(json& J, const FVector& V)
{
	J = json::array({ V.X, V.Y, V.Z });
}

inline void from_json(const json& J, FVector& V)
{
	for (int32 i = 0; i < 3; i++)
	{
		V[i] = J[i].get<float>();
	}
}

inline void to_json(json& J, const FRotator& V)
{
	J = json::array({ V.Pitch, V.Yaw, V.Roll });
}

inline void from_json(const json& J, FRotator& V)
{
	for (int32 i = 0; i < 3; i++)
	{
		V[i] = J[i].get<float>();
	}
}

inline void to_json(json& J, const FVector4& V)
{
	J = json::array({ V.X, V.Y, V.Z, V.W });
}

inline void from_json(const json& J, FVector4& V)
{
	for (int32 i = 0; i < 4; i++)
	{
		V[i] = J[i].get<float>();
	}
}

inline void to_json(json& J, const FTransform& T)
{
	J = json{ {"Location", T.Location}, {"Rotation", T.Rotation}, {"Scale", T.Scale} };
}

inline void from_json(const json& J, FTransform& T)
{
	J.at("Location").get_to(T.Location);
	J.at("Rotation").get_to(T.Rotation);
	J.at("Scale").get_to(T.Scale);
}