param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Binary,
    [Parameter(Mandatory = $false, Position = 1)]
    [string]$ExpectedVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

if ([string]::IsNullOrWhiteSpace($ExpectedVersion)) {
    $ExpectedVersion = ([System.IO.File]::ReadAllText((Join-Path $PSScriptRoot "../VERSION"))).Trim()
}

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("diskpurge-test-" + [guid]::NewGuid().ToString("N"))
$image = Join-Path $temporaryDirectory "disque-virtuel.img"
$audit = "$image.audit"
$tamperedAudit = "$audit.tampered"
$incompleteAudit = "$audit.incomplete"
$suffixAudit = "$audit.suffix"
$interposedAudit = "$audit.interposed"
$emptyAudit = "$audit.empty"
$nonAlignedImage = Join-Path $temporaryDirectory "non-aligne.img"
$emptyImage = Join-Path $temporaryDirectory "vide.img"
$hardlinkAudit = Join-Path $temporaryDirectory "audit-hardlink.jsonl"
$symbolicAudit = Join-Path $temporaryDirectory "audit-symbolique.jsonl"
$auditHardlink = Join-Path $temporaryDirectory "audit-lien-physique.jsonl"

New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

try {
    $bytes = New-Object byte[] 16384
    for ($index = 0; $index -lt $bytes.Length; $index++) {
        $bytes[$index] = 0xA5
    }
    [System.IO.File]::WriteAllBytes($image, $bytes)

    $version = (& $Binary --version 2>&1 | Out-String).Trim()
    Assert-True ($LASTEXITCODE -eq 0) "La commande --version a échoué."
    Assert-True ($version -eq $ExpectedVersion) "La version retournée est inattendue : $version"

    $inspection = & $Binary inspect $image 2>&1 | Out-String
    Assert-True ($LASTEXITCODE -eq 0) "L’inspection du fichier virtuel a échoué."
    $identifierLine = $inspection -split "`r?`n" | Where-Object { $_ -match '^Identifiant\s+:' } | Select-Object -First 1
    Assert-True ($null -ne $identifierLine) "L’identifiant stable est absent de l’inspection."
    $identifier = ($identifierLine -replace '^Identifiant\s+:\s*', '').Trim()
    Assert-True ($identifier.Length -gt 0) "L’identifiant stable est vide."

    $plan = & $Binary plan $image 2>&1 | Out-String
    Assert-True ($LASTEXITCODE -eq 0) "La planification a échoué."
    Assert-True ($plan -match 'Exécutable\s+: oui') "Le plan virtuel devrait être exécutable."
    Assert-True ($plan -match 'Méthode\s+: clear-zero') "La méthode virtuelle devrait être clear-zero."

    & $Binary plan $image --method pruge *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Une méthode invalide a été acceptée."

    & $Binary inspect $image --option-inconnue *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Une option d’inspection invalide a été acceptée."

    & $Binary plan $image --verify valeur-invalide *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Un mode de vérification invalide a été accepté."

    & $Binary plan $image --lab-mode *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Le binaire de publication a accepté le mode laboratoire."

    & $Binary plan $image --verify full --verify sample *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Une option de sécurité dupliquée a été acceptée."

    & $Binary erase $image --confirm incorrect *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Une confirmation invalide a été acceptée."

    & $Binary erase $image --confirm $identifier --verify full --audit $audit *> $null
    Assert-True ($LASTEXITCODE -eq 0) "L’effacement virtuel a échoué."

    $result = [System.IO.File]::ReadAllBytes($image)
    Assert-True (($result | Where-Object { $_ -ne 0 }).Count -eq 0) "Le fichier virtuel contient encore des octets non nuls."
    $auditContent = [System.IO.File]::ReadAllText($audit, [System.Text.Encoding]::UTF8)
    Assert-True ($auditContent.Contains('"statut":"réussi"')) "Le journal d’audit ne contient pas le succès attendu."
    Assert-True ($auditContent -match '"opération":"[0-9a-f]{64}"') "L’identifiant d’opération est absent du journal."
    Assert-True ($auditContent -match '"empreinte":"[0-9a-f]{64}"') "L’empreinte est absente du journal."

    $verification = & $Binary verify-audit $audit 2>&1 | Out-String
    Assert-True ($LASTEXITCODE -eq 0) "La vérification du journal valide a échoué."
    Assert-True ($verification -match 'Journal valide : 2 enregistrements') "Le journal devrait contenir deux enregistrements valides."

    $symbolicAuditCreated = $false
    try {
        New-Item -ItemType SymbolicLink -Path $symbolicAudit -Target $audit -ErrorAction Stop | Out-Null
        $symbolicAuditCreated = $true
    }
    catch {
    }
    if ($symbolicAuditCreated) {
        & $Binary verify-audit $symbolicAudit *> $null
        Assert-True ($LASTEXITCODE -ne 0) "Un point de réanalyse vers le journal a été accepté."
    }

    New-Item -ItemType HardLink -Path $auditHardlink -Target $audit | Out-Null
    & $Binary verify-audit $auditHardlink *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Un lien physique vers le journal a été accepté."
    Remove-Item -LiteralPath $auditHardlink -Force

    $tamperedContent = $auditContent.Replace('"statut":"réussi"', '"statut":"falsifié"')
    [System.IO.File]::WriteAllText($tamperedAudit, $tamperedContent, [System.Text.UTF8Encoding]::new($false))
    & $Binary verify-audit $tamperedAudit *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Un journal falsifié a été accepté."

    & $Binary erase $image --confirm $identifier --verify full --audit $tamperedAudit *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Une écriture a été ajoutée à un journal falsifié."

    $auditLines = [System.IO.File]::ReadAllLines($audit, [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText($incompleteAudit, $auditLines[0] + "`n", [System.Text.UTF8Encoding]::new($false))
    & $Binary verify-audit $incompleteAudit *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Une opération sans état terminal a été acceptée."

    [System.IO.File]::WriteAllLines($suffixAudit, ($auditLines | ForEach-Object { $_ + "suffixe-interdit" }), [System.Text.UTF8Encoding]::new($false))
    & $Binary verify-audit $suffixAudit *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Un suffixe non authentifié a été accepté."

    $interposedContent = $auditContent.Replace('","empreinte"', '","champ_inattendu":1,"empreinte"')
    [System.IO.File]::WriteAllText($interposedAudit, $interposedContent, [System.Text.UTF8Encoding]::new($false))
    & $Binary verify-audit $interposedAudit *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Un champ intercalé hors empreinte a été accepté."

    [System.IO.File]::WriteAllBytes($emptyAudit, [byte[]]::new(0))
    & $Binary verify-audit $emptyAudit *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Un journal vide a été accepté."

    $nonAlignedBytes = New-Object byte[] 8193
    for ($index = 0; $index -lt $nonAlignedBytes.Length; $index++) {
        $nonAlignedBytes[$index] = 0xA7
    }
    [System.IO.File]::WriteAllBytes($nonAlignedImage, $nonAlignedBytes)
    $nonAlignedInspection = & $Binary inspect $nonAlignedImage 2>&1 | Out-String
    $nonAlignedIdentifier = (($nonAlignedInspection -split "`r?`n" | Where-Object { $_ -match '^Identifiant\s+:' } | Select-Object -First 1) -replace '^Identifiant\s+:\s*', '').Trim()
    & $Binary erase $nonAlignedImage --confirm $nonAlignedIdentifier --verify full *> $null
    Assert-True ($LASTEXITCODE -eq 0) "L’effacement d’une taille non alignée a échoué."
    Assert-True (([System.IO.File]::ReadAllBytes($nonAlignedImage) | Where-Object { $_ -ne 0 }).Count -eq 0) "La taille non alignée contient encore des données."

    [System.IO.File]::WriteAllBytes($emptyImage, [byte[]]::new(0))
    $emptyInspection = & $Binary inspect $emptyImage 2>&1 | Out-String
    $emptyIdentifier = (($emptyInspection -split "`r?`n" | Where-Object { $_ -match '^Identifiant\s+:' } | Select-Object -First 1) -replace '^Identifiant\s+:\s*', '').Trim()
    & $Binary erase $emptyImage --confirm $emptyIdentifier --verify full *> $null
    Assert-True ($LASTEXITCODE -eq 0) "L’effacement d’un fichier vide a échoué."

    for ($index = 0; $index -lt $bytes.Length; $index++) {
        $bytes[$index] = 0x5A
    }
    [System.IO.File]::WriteAllBytes($image, $bytes)
    $currentInspection = & $Binary inspect $image 2>&1 | Out-String
    $currentIdentifier = (($currentInspection -split "`r?`n" | Where-Object { $_ -match '^Identifiant\s+:' } | Select-Object -First 1) -replace '^Identifiant\s+:\s*', '').Trim()
    $hashBefore = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
    & $Binary erase $image --confirm $currentIdentifier --audit $image *> $null
    Assert-True ($LASTEXITCODE -ne 0) "La cible a été acceptée comme son propre journal."
    $hashAfter = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
    Assert-True ($hashBefore -eq $hashAfter) "La cible a été modifiée par son propre journal refusé."

    New-Item -ItemType HardLink -Path $hardlinkAudit -Target $image | Out-Null
    & $Binary erase $image --confirm $currentIdentifier --audit $hardlinkAudit *> $null
    Assert-True ($LASTEXITCODE -ne 0) "Un lien physique vers la cible a été accepté comme journal."
    $hashAfterHardlink = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
    Assert-True ($hashBefore -eq $hashAfterHardlink) "La cible a été modifiée par son lien physique utilisé comme journal."

    Write-Output "Tests CLI réussis."
}
finally {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
