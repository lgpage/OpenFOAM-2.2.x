/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2012-2013 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "pressureTools.H"
#include "volFields.H"
#include "dictionary.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
defineTypeNameAndDebug(pressureTools, 0);
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

Foam::word Foam::pressureTools::pName() const
{
    word fieldName = pName_;

    if (calcTotal_)
    {
        fieldName = "total(" + fieldName + ")";
    }
    else
    {
        fieldName = "static(" + fieldName + ")";
    }

    if (calcCoeff_)
    {
        fieldName = fieldName + "_coeff";
    }

    return fieldName;
}


Foam::dimensionedScalar Foam::pressureTools::rhoScale
(
    const volScalarField& p
) const
{
    if (p.dimensions() == dimPressure)
    {
        return dimensionedScalar("1", dimless,  1.0);
    }
    else
    {
        return dimensionedScalar("rhoRef", dimDensity, rhoRef_);
    }
}


Foam::tmp<Foam::volScalarField> Foam::pressureTools::rho
(
    const volScalarField& p
) const
{
    if (p.dimensions() == dimPressure)
    {
        return p.mesh().lookupObject<volScalarField>(rhoName_);
    }
    else
    {
        return
            tmp<volScalarField>
            (
                new volScalarField
                (
                    IOobject
                    (
                        "rho",
                        p.mesh().time().timeName(),
                        p.mesh(),
                        IOobject::NO_READ,
                        IOobject::NO_WRITE
                    ),
                    p.mesh(),
                    dimensionedScalar("zero", dimDensity, rhoRef_)
                )
            );
    }
}


Foam::dimensionedScalar Foam::pressureTools::pRef() const
{
    dimensionedScalar value("pRef", dimPressure, 0.0);

    if (calcTotal_)
    {
        value.value() += pRef_;
    }

    return value;
}


Foam::tmp<Foam::volScalarField> Foam::pressureTools::pDyn
(
    const volScalarField& p
) const
{
    const fvMesh& mesh = refCast<const fvMesh>(obr_);

    tmp<volScalarField> tpDyn
    (
        new volScalarField
        (
            IOobject
            (
                "pDyn",
                mesh.time().timeName(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensionedScalar("zero", dimPressure, 0.0)
        )
    );

    if (calcTotal_)
    {
        const volVectorField& U = obr_.lookupObject<volVectorField>(UName_);

        tpDyn() == rho(p)*0.5*magSqr(U);
    }

    return tpDyn;
}


Foam::tmp<Foam::volScalarField> Foam::pressureTools::convertToCoeff
(
    const volScalarField& p
) const
{
    tmp<volScalarField> tCoeff(p);

    if (calcCoeff_)
    {
        tCoeff() -= dimensionedScalar("pInf", dimPressure, pInf_);

        const dimensionedScalar p0("p0", dimPressure, SMALL);
        const dimensionedVector U("U", dimVelocity, UInf_);
        const dimensionedScalar rho("rho", dimDensity, rhoInf_);

        tCoeff() /= 0.5*rho*magSqr(U) + p0;
    }

    return tCoeff;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::pressureTools::pressureTools
(
    const word& name,
    const objectRegistry& obr,
    const dictionary& dict,
    const bool loadFromFiles
)
:
    name_(name),
    obr_(obr),
    active_(true),
    pName_("p"),
    UName_("U"),
    rhoName_("rho"),
    rhoRef_(1.0),
    calcTotal_(false),
    pRef_(0.0),
    calcCoeff_(false),
    pInf_(0.0),
    UInf_(vector::zero),
    rhoInf_(0.0)
{
    // Check if the available mesh is an fvMesh, otherwise deactivate
    if (!isA<fvMesh>(obr_))
    {
        active_ = false;
        WarningIn
        (
            "pressureTools::pressureTools"
            "("
                "const word&, "
                "const objectRegistry&, "
                "const dictionary&, "
                "const bool"
            ")"
        )   << "No fvMesh available, deactivating." << nl
            << endl;
    }

    read(dict);
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::pressureTools::~pressureTools()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::pressureTools::read(const dictionary& dict)
{
    if (active_)
    {
        dict.readIfPresent("pName", pName_);
        dict.readIfPresent("UName", UName_);
        dict.readIfPresent("rhoName", rhoName_);

        const volScalarField& p = obr_.lookupObject<volScalarField>(pName_);

        if (p.dimensions() != dimPressure)
        {
            dict.lookup("rhoRef") >> rhoRef_;
        }

        dict.lookup("calcTotal") >> calcTotal_;
        if (calcTotal_)
        {
            dict.lookup("pRef") >> pRef_;
        }

        dict.lookup("calcCoeff") >> calcCoeff_;
        if (calcCoeff_)
        {
            dict.lookup("pInf") >> pInf_;
            dict.lookup("UInf") >> UInf_;
            dict.lookup("rhoInf") >> rhoInf_;
        }
    }
}


void Foam::pressureTools::execute()
{
    // Do nothing - only valid on write
}


void Foam::pressureTools::end()
{
    // Do nothing - only valid on write
}


void Foam::pressureTools::timeSet()
{
    // Do nothing - only valid on write
}


void Foam::pressureTools::write()
{
    if (active_)
    {
        const volScalarField& p = obr_.lookupObject<volScalarField>(pName_);

        volScalarField pResult
        (
            IOobject
            (
                pName(),
                obr_.time().timeName(),
                obr_,
                IOobject::NO_READ
            ),
            convertToCoeff(rhoScale(p)*p + pDyn(p) + pRef())
        );

        pResult.write();
    }
}


// ************************************************************************* //
