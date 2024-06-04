// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Copyright (C) 2024 by Agustin L. Alvarez. All rights reserved.
//
// This work is licensed under the terms of the MIT license.
//
// For a copy, see <https://opensource.org/licenses/MIT>.
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// [  HEADER  ]
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#include "Gateway.hpp"

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// [   CODE   ]
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

namespace Gameplay
{
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    Gateway::Gateway(Ref<Subsystem::Context> Context)
        : Activity(Context)
    {
        mState = State::Idle;
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnAttach()
    {
        ConstSPtr<UI::Service> Browser = GetSubsystem<UI::Service>();

        Browser->Register("doAccountLogin", [this](CPtr<const UI::Value> Parameters)
        {
            OnAccountLogin(Parameters[0], Parameters[1]);
            return UI::Value();
        });
        Browser->Register("doAccountCreate", [this](CPtr<const UI::Value> Parameters)
        {
            OnAccountCreate(Parameters[0], Parameters[1], Parameters[2]);
            return UI::Value();
        });
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnDetach()
    {
        ConstSPtr<UI::Service> Browser = GetSubsystem<UI::Service>();
        Browser->Unregister("doAccountLogin");
        Browser->Unregister("doAccountCreate");
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnResume()
    {
        ConstSPtr<UI::Service> Browser = GetSubsystem<UI::Service>();
        Browser->Call("setMainScreen", "templates/connect.html");
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnAccountLogin(CStr Username, CStr Password)
    {
        if (mState != State::Idle)
        {
            return;
        }

        mState    = State::Authenticate;
        mUsername = Username;
        mPassword = Password;

        SPtr<Network::Client> Session
            = GetSubsystem<Network::Service>()->Connect(kRemoteAddress, kRemotePort);
        Session->SetProtocol(shared_from_this());
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnAccountCreate(CStr Username, CStr Password, CStr Email)
    {
        if (mState != State::Idle)
        {
            return;
        }

        mState    = State::Create;
        mUsername = Username;
        mPassword = Password;
        mEmail    = Email;

        SPtr<Network::Client> Session
            = GetSubsystem<Network::Service>()->Connect(kRemoteAddress, kRemotePort);
        Session->SetProtocol(shared_from_this());
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnConnect(ConstSPtr<Network::Client> Session)
    {
        switch (mState)
        {
        case State::Idle:
            break;
        case State::Authenticate:
            Session->Write(GatewayAccountLogin(mUsername, mPassword));
            break;
        case State::Create:
            Session->Write(GatewayAccountRegister(mUsername, mPassword, mEmail));
            break;
        case State::Waiting:
            break;
        }

        mState = State::Waiting;
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnDisconnect(ConstSPtr<Network::Client> Session)
    {
        ConstSPtr<UI::Service> Browser = GetSubsystem<UI::Service>();

        switch (mState)
        {
        case State::Authenticate:
            Browser->Call("showAccountLoginError", "error_connection");
            break;
        case State::Create:
            Browser->Call("showAccountCreateError", "error_connection");
            break;
        default:
            break;
        }

        mState = State::Idle;
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnRead(ConstSPtr<Network::Client> Client, CPtr<UInt08> Bytes)
    {
        Reader Serializer(Bytes);
        do
        {
            switch (Serializer.ReadInt<UInt>())
            {
            case GatewayAccountError::k_ID:
                OnMessage(Client, GatewayAccountError(Serializer));
                break;
            }
        }
        while (Serializer.GetAvailable() > 0);
    }

    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    // -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    void Gateway::OnMessage(ConstSPtr<Network::Client> Client, Ref<const GatewayAccountError> Message)
    {
        ConstSPtr<UI::Service> Browser = GetSubsystem<UI::Service>();

        switch (Message.Type)
        {
        case GatewayAccountError::ID::Unknown:
            Browser->Call("showAccountLoginError", "error_account_unknown");
            break;
        case GatewayAccountError::ID::Mismatch:
            Browser->Call("showAccountLoginError", "error_account_mismatch");
            break;
        case GatewayAccountError::ID::Exist:
            Browser->Call("showAccountCreateError", "error_account_exist");
            break;
        }
    }
}